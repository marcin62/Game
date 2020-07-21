# Game
Language: C
# Multiproccesing game 
At first you suppose to instal ncurses library on your ubuntu to make this game work on your pc
# How to compile and run
Serwer:
```
gcc -o serwer server.c -lrt -lpthread -lncurses
./serwer
```
Player:
```
gcc -o player player.c -lrt -lpthread -lncurses
./player
```
Bot:
```
gcc -o bot bot.c -lrt -lpthead -lncurses
./bot
```
