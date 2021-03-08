Installation
============

Minimum System Requirements
^^^^^^^^^^^^^^^^^^^^^^^^^^^

Titan has been tested on Ubuntu 18.04 LTS. It is advisable to run it on a 
system or VM with atleast 4 cores and 4GB of RAM.

* Default python version on the system must be >= 3.6.9

Installing Titan
^^^^^^^^^^^^^^^^

To get started, please perform the following installation steps:

.. Clone Repository into $HOME directory. Checkout the master branch::

.. git clone https://github.com/Vignesh2208/Titan.git

* Download version 1.0 of the code from `here <https://github.com/Vignesh2208/Titan/archive/v1.0.tar.gz>`_ and extract it to the $HOME folder under a directory with the name **Titan**.

* (Optional) Edit any build configs if necessary. These are located in the 
  CONFIG file. Unless required, its safe to leave this untouched::

    vim ~/Titan/CONFIG (edit as necessary)

  For more information, refer to the build CONFIG flags described inside 
  the file.

* Configure and install Titan::
 
    cd ~/Titan 
    sudo ./SETUP.sh install_deps
    sudo ./SETUP.sh build
    sudo ./SETUP.sh install


* (Optional) If you need to change the build CONFIGs after installation::

    cd ~/Titan
    vim CONFIG (edit as necessary)
    sudo ./SETUP.sh clean
    sudo ./SETUP.sh reinstall

* Post installation. Open a new terminal and run the following command::

    ttn init

* Load Titan module after each reboot::
 
    cd ~/Titan
    sudo make load
    
Ready to use VM
^^^^^^^^^^^^^^^

Instuctions to Follow.
