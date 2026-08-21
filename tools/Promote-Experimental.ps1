# Merges experimental -> main in UI, HALO, and I&A.
# Always restores production addon.gproj so live Workshop GUIDs are not retargeted.
# Does not push. Does not touch *-Exp worktrees.

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$EngineGuid = "58D0FB3206B6F859"

$Repos = @(
	@{
		Name = "Mike's UI"
		Path = "F:\Mikes-UI"
		ProductionGuid = "B3F91C6A4E275D08"
		ProductionId = "MikesUI"
	}
	@{
		Name = "Mike's HALO Jump"
		Path = "F:\Mikes-HALO-Jump"
		ProductionGuid = "C4E8A27B1F906D53"
		ProductionId = "MikesHALOJump"
	}
	@{
		Name = "Mikes Invade and Annex"
		Path = "F:\Mikes-Invade-and-Annex"
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

		$status = git status --porcelain
		if ($status)
		{
			throw "$($Repo.Name) has uncommitted changes. Commit or stash them in the production folder before promoting."
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
			git merge --abort
			throw "$($repo.Name) merge failed. Ran merge --abort. Resolve conflicts on experimental, then rerun."
		}

		git checkout HEAD -- addon.gproj
		if ($LASTEXITCODE -ne 0)
		{
			git merge --abort
			throw "$($repo.Name) failed to restore production addon.gproj. Ran merge --abort."
		}

		Assert-ProductionIdentity $repo
		git add -- addon.gproj

		$staged = git diff --cached --name-only
		if (-not $staged)
		{
			git merge --abort
			Write-Host "$($repo.Name): nothing to promote besides identity (already skipped). Production addon.gproj left unchanged."
			continue
		}

		git commit -m "Merge experimental into main; keep production Workshop identity"
		if ($LASTEXITCODE -ne 0)
		{
			git merge --abort
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
