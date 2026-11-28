PyInit
=====

PyInit embeds a Windows x64 CPython runtime inside pyinit.shpak.
It provides persistent Python globals across multiple $initpy blocks.

Build:
  1. Put the updated shentrypak.exe beside build.bat (or build it at ..\\build\\shentrypak.exe).
  2. Run build.bat. It downloads CPython 3.13.7 x64, embeds the runtime files,
     creates pyinit.shpak with shentrypak --resource-dir, and installs it.

main.shay is the package manifest. The actual executable runtime is bundled
from runtime/ and host.py; the package is not just a text library.

Use in a Shay program:
  $useentry pyinit

  instxt/>"Hello from Shay"

  $initpy
  message = "Hello from Python"
  print(message)
  $endpy

  $initpy
  print(message + " again")
  $endpy

Requirements:
  - Windows x64
  - Updated shentrypak.exe and shaii.exe from the same Shay build
  - Internet access once, to download the official CPython embeddable runtime

Python packages installed with pip are not included; this uses the official
Python embeddable distribution and its standard library.
