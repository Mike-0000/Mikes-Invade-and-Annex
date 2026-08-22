# Merges experimental -> main in UI, HALO, and I&A.
# Always restores production addon.gproj so live Workshop GUIDs are not retargeted.
# Restores Workbench resourceDatabase.rdb dirt in production and *-Exp folders.
# Does not push. Does not merge or commit in *-Exp worktrees.

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$EngineGuid = "58D0FB3206B6F859"
$IgnorableDirty = @("resourceDatabase.rdb")

$Repos = @(
	@{
		Name = "Mike's UI"
		Path = "F:\Mikes-UI"
		ExpPath = "F:\Mikes-UI-Exp"
		ProductionGuid = "B3F91C6A4E275D08"
		ProductionId = "MikesUI"
	}
	@{
		Name = "Mike's HALO Jump"
		Path = "F:\Mikes-HALO-Jump"
		ExpPath = "F:\Mikes-HALO-Jump-Exp"
		ProductionGuid = "C4E8A27B1F906D53"
		ProductionId = "MikesHALOJump"
	}
	@{
		Name = "Mikes Invade and Annex"
		Path = "F:\Mikes-Invade-and-Annex"
		ExpPath = "F:\Mikes-Invade-and-Annex-Exp"
		ProductionGuid = "6556458885927F2F"
		ProductionId = "MikesInvadeandAnnex"
	}
)

function Get-GprojIdentity([string] $GprojPath)
{
	if (-not (Test-Path -LiteralPath $GprojPath))
	{
		throw "Missing $GprojPath"
	}

	$content = Get-Content -LiteralPath $GprojPath -Raw
	$idMatch = [regex]::Match($content, 'ID\s+"([^"]+)"')
	$guidMatch = [regex]::Match($content, 'GUID\s+"([^"]+)"')
	if (-not $idMatch.Success -or -not $guidMatch.Success)
	{
		throw "Could not parse ID/GUID in $GprojPath"
	}

	return @{
		Id = $idMatch.Groups[1].Value
		Guid = $guidMatch.Groups[1].Value
	}
}

function Get-PorcelainPaths([string] $RepoPath)
{
	$paths = @()
	if (-not (Test-Path -LiteralPath $RepoPath))
	{
		return $paths
	}

	Push-Location -LiteralPath $RepoPath
	try
	{
		$lines = @(git status --porcelain)
		foreach ($line in $lines)
		{
			if (-not $line -or $line.Length -lt 4)
			{
				continue
			}

			$paths += $line.Substring(3).Trim().Trim('"')
		}
	}
	finally
	{
		Pop-Location
	}

	return $paths
}

function Restore-IgnorableDirt([string] $RepoPath)
{
	if (-not $RepoPath -or -not (Test-Path -LiteralPath $RepoPath))
	{
		return
	}

	$dirty = @(Get-PorcelainPaths $RepoPath)
	foreach ($file in $dirty)
	{
		if ($IgnorableDirty -notcontains $file)
		{
			continue
		}

		Push-Location -LiteralPath $RepoPath
		try
		{
			Write-Host "Restoring ignorable Workbench file in ${RepoPath}: $file"
			git restore -- $file
			if ($LASTEXITCODE -ne 0)
			{
				throw "Failed to restore $file in $RepoPath"
			}
		}
		finally
		{
			Pop-Location
		}
	}
}

function Test-MergeInProgress
{
	git rev-parse -q --verify MERGE_HEAD 2>$null | Out-Null
	return ($LASTEXITCODE -eq 0)
}

function Assert-ProductionIdentity($Repo)
{
	$gproj = Join-Path $Repo.Path "addon.gproj"
	$identity = Get-GprojIdentity $gproj
	if ($identity.Guid -ne $Repo.ProductionGuid -or $identity.Id -ne $Repo.ProductionId)
	{
		throw "$($Repo.Name) addon.gproj is not production identity. Expected ID $($Repo.ProductionId) GUID $($Repo.ProductionGuid), found ID $($identity.Id) GUID $($identity.Guid). Aborting so live Workshop listings are not retargeted."
	}
}

function Assert-CleanMain($Repo)
{
	Push-Location -LiteralPath $Repo.Path
	try
	{
		$branch = (git rev-parse --abbrev-ref HEAD).Trim()
		if ($branch -ne "main")
		{
			throw "$($Repo.Name) is on '$branch', not main. Checkout main in the production folder first."
		}

		$dirty = @(Get-PorcelainPaths $Repo.Path)
		if ($dirty.Count -gt 0)
		{
			throw "$($Repo.Name) has uncommitted changes: $($dirty -join ', '). Commit or discard real files in the production folder before promoting."
		}

		git show-ref --verify --quiet refs/heads/experimental
		if ($LASTEXITCODE -ne 0)
		{
			throw "$($Repo.Name) has no local experimental branch."
		}
	}
	finally
	{
		Pop-Location
	}
}

Write-Host "Checking production identities and working trees..."
foreach ($repo in $Repos)
{
	Restore-IgnorableDirt $repo.Path
	Restore-IgnorableDirt $repo.ExpPath
	Assert-ProductionIdentity $repo
	Assert-CleanMain $repo
}

foreach ($repo in $Repos)
{
	Write-Host ""
	Write-Host "Promoting $($repo.Name)..."
	Push-Location -LiteralPath $repo.Path
	try
	{
		git merge experimental --no-commit --no-ff
		if ($LASTEXITCODE -ne 0)
		{
			$conflicts = @(git diff --name-only --diff-filter=U)
			$list = "none"
			if ($conflicts.Count -gt 0)
			{
				$list = $conflicts -join ", "
			}

			if (Test-MergeInProgress)
			{
				git merge --abort
			}

			throw "$($repo.Name) merge failed. Ran merge --abort. CONFLICT_FILES: $list. Merge main into experimental in $($repo.ExpPath), resolve, commit, then rerun."
		}

		git checkout HEAD -- addon.gproj
		if ($LASTEXITCODE -ne 0)
		{
			if (Test-MergeInProgress)
			{
				git merge --abort
			}

			throw "$($repo.Name) failed to restore production addon.gproj. Ran merge --abort."
		}

		Assert-ProductionIdentity $repo
		git add -- addon.gproj

		$staged = git diff --cached --name-only
		if (-not $staged)
		{
			if (Test-MergeInProgress)
			{
				git merge --abort
			}

			Write-Host "$($repo.Name): nothing to promote besides identity (already skipped). Production addon.gproj left unchanged."
			continue
		}

		git commit -m "Merge experimental into main; keep production Workshop identity"
		if ($LASTEXITCODE -ne 0)
		{
			if (Test-MergeInProgress)
			{
				git merge --abort
			}

			throw "$($repo.Name) merge commit failed. Ran merge --abort."
		}

		Assert-ProductionIdentity $repo
		Write-Host "$($repo.Name): merged. Production GUID $($repo.ProductionGuid) kept."
	}
	finally
	{
		Pop-Location
	}
}

Write-Host ""
Write-Host "Done. Open the production folders in Workbench and publish in order: UI, HALO, I&A."
Write-Host "Engine dependency remains $EngineGuid."
