#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <iostream>
#include <vector>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <fstream>
#include <chrono>
#include <thread>

using namespace std;

constexpr size_t PACKET_SIZE = 1024*5; // 5 kilo-bytes

constexpr bool DEBUG = false;

struct Packet {
    int number;
    char packet[PACKET_SIZE];
};

int main() {

    std::cout << "** == SERVER APPLICATION == **\n\n";

    SOCKET server_socket;
    int port = 55555;
    WSADATA wsa_data;
    int wsa_err;

    // intializing Windows socket programming API
    WORD w_version_required = MAKEWORD(2, 2);
    wsa_err = WSAStartup(w_version_required, &wsa_data);
    if (wsa_err != 0) {
        cout << "The winsock dll not found." << endl;
        return 0;
    }

    // setting up UDP socket
    server_socket = INVALID_SOCKET;
    server_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (server_socket == INVALID_SOCKET) {
        cout << "Error at socket: " << WSAGetLastError() << endl;
        WSACleanup();
        return 0;
    }

    // binding the socket to address and port
    sockaddr_in server_addrr;
    server_addrr.sin_family = AF_INET;
    server_addrr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addrr.sin_port = htons(port);

    if (bind(server_socket, (SOCKADDR*)&server_addrr, sizeof(server_addrr)) == SOCKET_ERROR) {
        cout << "bind() failed: " << WSAGetLastError() << endl;
        closesocket(server_socket);
        WSACleanup();
        return 0;
    }

    std::cout << "** == Waiting for CLIENT request == **\n\n";

    char receive_buffer[200];
    sockaddr_in client_address;
    int client_address_length = (int)sizeof(client_address);
    int  byte_received = recvfrom(server_socket, receive_buffer, strlen(receive_buffer), 0, (struct sockaddr *)&client_address, &client_address_length);
    if (byte_received < 0) {
        printf("Cound not receive datagram.\n");
        WSACleanup();
        return 0;
    }
    else {

        receive_buffer[byte_received] = 0; // adding ednline char after the last byte recieved
        printf("Received request to download: %s \n\n", receive_buffer);

        // opening the file
        ifstream file(receive_buffer, ios::binary|ios::ate);

        if (!file) {
            cout << "Recived invalid file name. Terminating. \n";
            return 1;
        }

        int file_size = file.tellg(); 
        cout << "File size: " << file_size << " Bytes" << "\n";
        file.seekg(0, std::ios::beg);

        char buff[PACKET_SIZE];
        int total_packets = (file_size / PACKET_SIZE)+1;

        cout << "Total packets to be sent: " << total_packets << "\n\n";
        
        // sending file size to the clients
        sendto(server_socket, (const char*)&file_size, sizeof(int), 0, (struct sockaddr*)&client_address, client_address_length);

        Packet packet;
        int cur_packet = 1;

        while (cur_packet <= total_packets)
        {   
            // if reached at last packet, then read only remaining bytes
            if(cur_packet == total_packets) 
                file.read(packet.packet, file_size%PACKET_SIZE);
            else
                file.read(packet.packet, PACKET_SIZE);
            
            auto bytesRead = file.gcount();
            if (DEBUG) cout << "Reading packet # " << cur_packet << ", [bytes read from file: " << bytesRead << "]" << endl;
            if (bytesRead == 0) break;

            // setting packet number
            packet.number=cur_packet;
            
            send_ith_packet: // sending current packet to client            
            int byte_sent = sendto(server_socket, (const char*)&packet, sizeof(Packet), 0, (struct sockaddr*)&client_address, client_address_length);
            if (byte_sent == -1) {
                if (DEBUG) printf("Error sending packet # %d.\n", cur_packet);
                WSACleanup();
                return 0;
            }
            else {
                if (DEBUG) printf("Sent packet # %d. Waiting for ack.\n", cur_packet);
            }

            // wait for ack of above sent packet
            byte_received = recvfrom(server_socket, (char *)&receive_buffer, sizeof(int), 0, (struct sockaddr *)&client_address, &client_address_length);

            if (byte_received < 0) {
                if (DEBUG) printf("Cound not receive acknowledgment for packet # %d.\n", cur_packet);
            }
            else {
                int ackNumber = ((int * ) receive_buffer)[0];
                if (DEBUG) printf("Received acknowlegdment for packet # %d\n\n", ackNumber);
                if(ackNumber != cur_packet){
                    goto send_ith_packet;
                }
            }

            //std::this_thread::sleep_for(std::chrono::milliseconds(2000));
            
            cur_packet++;
        }

        file.close();
    }

    system("pause");

    WSACleanup();

    return 0;
}
