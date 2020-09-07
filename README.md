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
# Server details
Cpu and humans players are separated processes. Server is designed for maximum 4 clients. If user try to execute more clients, server will reject to join him. When closing the server, we also close all clients. After the client quits server is ready to join next client on his place. This communication is ensured by mechanisms like semaphores, shared memory and threads.

Clients can affect on game only by move requests. Server receives this requests and processes this move, by updating board and game. Server passes information to clients about theirs surrounding, status and statistics. They can change them only locally.

# About game
Server input:
```
B/b – add new beast to game
c/t/T – add new reward to game
Q/q – close server and clients
```
Human input:
```
Arrows – move
Q/q – close client
```
Client goal is to collect the rewards and store them in campside. Client cannot go through wall and bushes slow him down. In case of collision with other client or beast, client lost his treasure and leave on the map. 
### Server view
![img](C:\Users\Marcin\Desktop\Serwer.png)
### Client view ( 4 clients in 4 terminals ) 
![img](/Klient.png)
