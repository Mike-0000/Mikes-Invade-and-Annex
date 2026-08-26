# One-shot Exp (or I&A cursor/tools) commit. Avoids the six-step status/diff/log/HEREDOC round-trips.
# Does not push. Does not promote. Skips addon.gproj and resourceDatabase.rdb.

[CmdletBinding()]
param(
	[Parameter(Mandatory = $true)]
	[string]$Repo,

	[Parameter(Mandatory = $true)]
	[string]$Message,

	[string[]]$Files
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$SkipNames = New-Object "System.Collections.Generic.HashSet[string]" ([StringComparer]::OrdinalIgnoreCase)
[void]$SkipNames.Add("addon.gproj")
[void]$SkipNames.Add("resourceDatabase.rdb")

$Allowed = @{
	"F:\Mikes-UI-Exp" = "experimental"
	"F:\Mikes-HALO-Jump-Exp" = "experimental"
	"F:\Mikes-Invade-and-Annex-Exp" = "experimental"
	"F:\Mikes-Invade-and-Annex" = "main"
}

$ProductionIa = "F:\Mikes-Invade-and-Annex"

function Get-FullPath([string]$Path)
{
	return [IO.Path]::GetFullPath($Path).TrimEnd("\", "/")
}

function Get-RepoRelative([string]$RepoPath, [string]$Path)
{
	$repoRoot = (Get-FullPath $RepoPath) + [IO.Path]::DirectorySeparatorChar
	$full = Get-FullPath $Path
	if (-not $full.StartsWith($repoRoot, [StringComparison]::OrdinalIgnoreCase))
	{
		throw "File is outside repo ${RepoPath}: $Path"
	}

	return $full.Substring($repoRoot.Length).Replace("\", "/")
}

function Test-ProductionCursorOrTools([string]$RelativePath)
{
	$norm = $RelativePath.Replace("\", "/")
	if ($norm.StartsWith(".cursor/", [StringComparison]::OrdinalIgnoreCase))
	{
		return $true
	}
	if ($norm.StartsWith("tools/", [StringComparison]::OrdinalIgnoreCase))
	{
		return $true
	}

	return $false
}

function Get-ChangedPaths
{
	$paths = New-Object System.Collections.Generic.List[string]
	$lines = @(git status --porcelain -u --untracked-files=all)
	foreach ($line in $lines)
	{
		if (-not $line -or $line.Length -lt 4)
		{
			continue
		}

		$path = $line.Substring(3).Trim().Trim('"')
		if ($path -match " -> ")
		{
			$parts = $path -split " -> "
			$path = $parts[$parts.Length - 1].Trim().Trim('"')
		}

		$paths.Add($path.Replace("\", "/"))
	}

	return $paths
}

$repoPath = Get-FullPath $Repo
$expectedBranch = $null
foreach ($key in $Allowed.Keys)
{
	if ((Get-FullPath $key) -eq $repoPath)
	{
		$expectedBranch = $Allowed[$key]
		break
	}
}

if (-not $expectedBranch)
{
	throw "Repo is not an allowed commit folder: $repoPath"
}

$isProductionIa = ((Get-FullPath $ProductionIa) -eq $repoPath)

Push-Location -LiteralPath $repoPath
try
{
	$branch = (git rev-parse --abbrev-ref HEAD).Trim()
	if ($branch -ne $expectedBranch)
	{
		throw "Expected branch $expectedBranch, found '$branch' in $repoPath"
	}

	if (Test-Path -LiteralPath "resourceDatabase.rdb")
	{
		git restore -- resourceDatabase.rdb 2>$null | Out-Null
	}

	$fileList = New-Object System.Collections.Generic.List[string]
	if ($PSBoundParameters.ContainsKey("Files") -and $Files -and $Files.Count -gt 0)
	{
		foreach ($file in $Files)
		{
			if (-not $file)
			{
				continue
			}

			foreach ($part in ($file -split ","))
			{
				$trimmed = $part.Trim().Trim('"')
				if ($trimmed)
				{
					$fileList.Add($trimmed)
				}
			}
		}
	}

	$toAdd = New-Object System.Collections.Generic.List[string]
	if ($fileList.Count -gt 0)
	{
		foreach ($file in $fileList)
		{
			$name = Split-Path -Leaf $file
			if ($SkipNames.Contains($name))
			{
				continue
			}

			$full = $file
			if (-not [IO.Path]::IsPathRooted($file))
			{
				$full = Join-Path $repoPath $file
			}

			$rel = Get-RepoRelative $repoPath $full
			if ($isProductionIa -and -not (Test-ProductionCursorOrTools $rel))
			{
				throw "Production I&A commits are limited to .cursor/ and tools/: $rel"
			}

			$toAdd.Add($rel)
		}
	}
	else
	{
		foreach ($path in @(Get-ChangedPaths))
		{
			$name = Split-Path -Leaf $path
			if ($SkipNames.Contains($name))
			{
				continue
			}

			if ($isProductionIa -and -not (Test-ProductionCursorOrTools $path))
			{
				continue
			}

			$toAdd.Add($path)
		}
	}

	if ($toAdd.Count -eq 0)
	{
		Write-Output "SKIPPED nothing to commit"
		exit 0
	}

	git add -- @($toAdd)
	if ($LASTEXITCODE -ne 0)
	{
		throw "git add failed in $repoPath"
	}

	$staged = @(git diff --cached --name-only)
	if ($staged.Count -eq 0)
	{
		Write-Output "SKIPPED nothing staged"
		exit 0
	}

	$tmp = [IO.Path]::GetTempFileName()
	try
	{
		[IO.File]::WriteAllText($tmp, ($Message.Trim() + "`n"))
		git commit -F $tmp
		if ($LASTEXITCODE -ne 0)
		{
			throw "git commit failed in $repoPath"
		}
	}
	finally
	{
		Remove-Item -LiteralPath $tmp -Force -ErrorAction SilentlyContinue
	}

	$hash = (git rev-parse --short HEAD).Trim()
	$names = @(git diff-tree --no-commit-id --name-only -r HEAD)
	Write-Output "COMMITTED $hash"
	foreach ($name in $names)
	{
		Write-Output $name
	}
}
finally
{
	Pop-Location
}
