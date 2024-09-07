## User Interfaces for using OAuth2 with Printers and Scanners

This repository contains the components of my Google Summer of Code 2024 for OpenPrinting @ The Linux Foundation.

These libraries allow the CPDB to communicate with clients and verify their identity to keep client data save. The libraries used for adding OAuth support to CPDB for printers and scanners are the inbuilt libraries of GNOME Online Accounts (GOA).

## Background

The [Common Print Dialog Backends](https://openprinting.github.io/achievements/#common-print-dialog-backends) roject aims to move the responsability on the part of the print dialog which communicates with the print system away from the GUI toolkit/app developers to the print system's developers and also to bring all print technologies available to the user (CUPS, cloud printing services, ...) into all application's print dialogs.
# The Problem

Printing out of desktop applications is managed by many different dialogs, mostly depending on which GUI toolkit is used for an application. Some applications even ask users to provide authentication URL for authentication. This create a problem for the user.

# Solution

The OAuth support project aims to solve these problems by providing the authentication URL and by giving them plenty of ways to authenticate their identities.

## Dependencies

 - [cpdb-libs] (https://github.com/OpenPrinting/cpdb-libs): Version >= 2.0.0 (or GIT Master)
 - [CUPS] (https://github.com/OpenPrinting/cups): version>=2.5 or `sudo apt install cups libcups2-dev`
 - GLIB 2.0: `sudo apt install libglib2.0-dev`

## Setup

To install all the necessary dependencies and compile the project, run the following script:

```sh
./setup.sh
```

## Build and Installation

 ```
 $ ./autogen
 $ ./configure
 $ make
 $ sudo make install
 $ sudo ldconfig
```

## Testing the library

To test the UI and run the server you can use the following command and one demo file is also created to test the code in the local machine.

```
 $ cd CPDB_Backend_OAuthSupport/OAuth
 $ make
 $ ./Auth
 $ ./AuthUI
 $ ./auth_server
```
# Updates and development

The more info and complete source code you find on the [OpenPrinting GitHub.](https://github.com/OpenPrinting/cpdb-backend-cups).

## More Info

This repository contains the source code of my contribution in Google Summer of Code 2024 and here's my [GSOC'24 Project Report]()
