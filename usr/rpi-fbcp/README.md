Raspberry Pi Framebuffer Copy
=============================
This program used for copy primary framebuffer to secondary framebuffer (eg. FBTFT). It require lastest raspberry pi firmware (> 2013-07-11) to working properly.

Tested on Raspberry Pi 3
========================
2017-11-29-raspbian-stretch


Requirement
-----------
cmake

$ sudo apt-get install cmake

Build
-----

    $ mkdir build
    
    $ cd build
    
    $ cmake ..
    
    $ make 


How To Use
----------
$ ./fbcp

Wanna to run from booting
-------------------------
$ sudo cp fbcp /usr/bin
$ sudo chmod +x /usr/bin/fbcp
$ sudo nano /etc/rc.local -> add new line before "exit 0" with "/usr/bin/fbcp &" without quote
$ sudo reboot


License
-------


Copyright (c) 2013 Tasanakorn Phaipool

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.