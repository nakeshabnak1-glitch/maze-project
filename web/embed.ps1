# Embed maze.wasm (base64) into index.template.html and write index.html.
# The result is a single file that works when double-clicked (file://), no server needed.
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$wasm = [Convert]::ToBase64String([IO.File]::ReadAllBytes((Join-Path $here "maze.wasm")))
$html = [IO.File]::ReadAllText((Join-Path $here "index.template.html"), [Text.Encoding]::UTF8)
if (-not $html.Contains("__MAZE_WASM_BASE64__")) { Write-Error "index.template.html has no __MAZE_WASM_BASE64__ placeholder"; exit 1 }
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[IO.File]::WriteAllText((Join-Path $here "index.html"), $html.Replace("__MAZE_WASM_BASE64__", $wasm), $utf8NoBom)
Write-Host ("index.html written, wasm " + (Get-Item (Join-Path $here "maze.wasm")).Length + " bytes")
