# Game
C
# Multiproccesing game 
At first you suppose to instal lncurses library on your ubuntu to make this game work on your pc
# How to compile and run
Serwer:
gcc -o serwer server.c -lrt -lpthread -lncurses
./serwer

Gracz:
gcc -o player player.c -lrt -lpthread -lncurses
./player

Bot:
gcc -o bot bot.c -lrt -lpthead -lncurses
./bot