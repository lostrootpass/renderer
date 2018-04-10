Get-ChildItem -Recurse -Path . | Where-Object {$_.Name -match '.(frag|vert)$'} | ForEach-Object {& "${env:VULKAN_SDK}\Bin\glslangValidator.exe" -V $_.FullName -o "$($_.fullname).spv"}
