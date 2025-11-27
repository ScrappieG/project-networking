# Testing

1. update commoncfg
   - if you are testing with one computer in different terminals then keeping localhost in PeerInfo.cfg is good.
   - If you are testing with multiple computers you will need to find the ip of the other computers
   - to find ip on linux/macOS I used ifconfig | grep "inet " | grep -v 127.0.0.1 (windows will probably be different).

2. Run make clean -> make to compile the program.

3. make sure directories are set up correctly

   peerProcess.exe
   
   |-/src

   |-/peer_100*/file

5. run ./peerProcess (number) in ascending order
