WHP-simple
==========

A simple example of using Windows Hypervisor Platform (WHP)

## Preparation

### 1. Type the following command in PowerShell as the administrator to enable WHP.

`Enable-WindowsOptionalFeature -Online -FeatureName HypervisorPlatform`

### 2. Download and install the latest Windows SDK from the following site.

https://developer.microsoft.com/windows/downloads/windows-10-sdk/

## Build & Run

### Visual Studio

Open `WHP-simple.sln` with Visual Studio (2019) and press F5 to run.

### Cygwin

Type `make` to build and `./main` to run.

