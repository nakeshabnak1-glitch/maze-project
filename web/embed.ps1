# Embed the WebAssembly module and benchmark data in the standalone HTML page.
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$docs = Join-Path (Split-Path -Parent $here) "docs"
$html = [IO.File]::ReadAllText((Join-Path $here "index.template.html"), [Text.Encoding]::UTF8)
$assets = @{
    "__MAZE_WASM_BASE64__" = Join-Path $here "maze.wasm"
    "__PERF_CSV_BASE64__" = Join-Path $docs "perf_results.csv"
    "__LOOP_CSV_BASE64__" = Join-Path $docs "loop_results.csv"
}
foreach ($entry in $assets.GetEnumerator()) {
    if (-not $html.Contains($entry.Key)) { throw "Missing template placeholder: $($entry.Key)" }
    $data = [Convert]::ToBase64String([IO.File]::ReadAllBytes($entry.Value))
    $html = $html.Replace($entry.Key, $data)
}
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[IO.File]::WriteAllText((Join-Path $here "index.html"), $html, $utf8NoBom)
Write-Host "index.html written with WebAssembly and benchmark data."
