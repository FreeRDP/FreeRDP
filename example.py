# Create a folder under the drive root
$ mkdir actions-runner; cd actions-runner# Download the latest runner package
$ Invoke-WebRequest -Uri https://github.com/actions/runner/releases/download/v2.298.2/actions-runner-win-x64-2.298.2.zip -OutFile actions-runner-win-x64-2.298.2.zip# Optional: Validate the hash
$ if((Get-FileHash -Path actions-runner-win-x64-2.298.2.zip -Algorithm SHA256).Hash.ToUpper() -ne '02c11d07fcc453f95fc5c15e11ea911cd0fd56f595bd70f2e8df87f46b2c796a'.ToUpper()){ throw 'Computed checksum did not match' }# Extract the installer
$ Add-Type -AssemblyName System.IO.Compression.FileSystem ; [System.IO.Compression.ZipFile]::ExtractToDirectory("$PWD/actions-runner-win-x64-2.298.2.zip", "$PWD")
