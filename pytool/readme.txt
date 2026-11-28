PyInit
=====

PyInit embeds a Windows x64 CPython runtime inside pyinit.shpak.
It provides persistent Python globals across multiple $initpy blocks.

Build:
  build.bat

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
  - shentrypak.exe and shaii.exe from the same updated Shay build
  - Internet access once, to download the official CPython embeddable runtime

Python packages installed with pip are not included; this uses the official
Python embeddable distribution and its standard library.
