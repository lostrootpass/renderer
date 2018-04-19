Get-ChildItem -Recurse -Path . | Where-Object {$_.Name -match '.(frag|vert)$'} | ForEach-Object {& "${env:VULKAN_SDK}\Bin\glslc.exe" -I $_.Directory -o "$($_.fullname).spv" $_.FullName }
