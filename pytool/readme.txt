PyInit
=====

PyInit embeds a Windows x64 CPython runtime inside pyinit.shpak.
It provides persistent Python globals across multiple $initpy blocks.

Build:
  1. Put the updated shentrypak.exe beside build.bat (or build it at ..\\build\\shentrypak.exe).
  2. Run build.bat. It downloads CPython 3.13.7 x64, embeds the runtime files,
     creates pyinit.shpak with shentrypak --resource-dir, and installs it.

main.shay defines the pyinitinfo package function. build.bat embeds the actual
CPython runtime files from runtime/ and the host.py executor into the .shpak.

Use in a Shay program:
  $useentry pyinit
  pyinitinfo

  instxt/>"Hello from Shay"

  $initpy
  message = "Hello from Python"
  print(message)
  $endpy

  $initpy
  print(message + " again")
  $endpy

Run Python-containing programs with the updated shaii.exe interpreter, not gshai.
The gshai CLI and compiler remain unchanged.

Requirements:
  - Windows x64
  - Updated shentrypak.exe and shaii.exe from the same Shay build
  - Internet access once, to download the official CPython embeddable runtime

Python packages installed with pip are not included; this uses the official
Python embeddable distribution and its standard library.
