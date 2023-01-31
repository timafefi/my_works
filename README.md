# my_works
Some programs that I have developed throughout my programming education


Here is a description of how each program in this repository works:

1) shell.c:

Emulates the behaviour of Linux Shell. The follwing features of the original Shell are included:

-all the usual Shell commands (using chdir() and exec() system calls)

-executing commands in backgound by adding an ampersand to the end (e.g.  "firefox&")

-executing commands in a conveyer (e.g. "ps ax | grep ...")

-input/output redirection to a file (e.g. "./prog > result < data")


2)server.c

TCP/IP socket server that allows multiple users to simultaneously execute simple commands

3)manager_game.c

A multiplayer console game written using the server above. The rules of the game are described in the book "Etudes for programmers", Charles Wetherell.

Startup example:

./game [port] [amount of players] 

telnet 0 [port]

4)game_manager_robot.cpp

A simple bot that connects to the server and plays the manager_game using a simplest strategy

5)domain_reg.py

A program that automatizes the process of registrating domain names using Selenium Webdriver module.

Input data:

registrator_url:email:passoword:domain_name:profile_name:profile_email:dns1:dns2:[dns3]:[dns4]
