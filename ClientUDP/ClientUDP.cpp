// NetworkClient.cpp : This file contains the 'main' function. Program execution begins and ends there.

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <iostream>
#include <istream>
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

int main()
{
    std::cout << "** == CLIENT APPLICATION == **\n\n";

    // intializing Windows socket programming API
    SOCKET client_socket;
    int port = 55555;
    WSADATA wsa_data;
    int wsa_err;
    WORD w_version_requested = MAKEWORD(2, 2);
    wsa_err = WSAStartup(w_version_requested, &wsa_data);
    if (wsa_err != 0) {
        cout << "The winsok dll not found." << endl;
        return 0;
    }

    // setting up UDP socket
    client_socket = INVALID_SOCKET;
    client_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (client_socket == INVALID_SOCKET) {
        cout << "Error at socket(): " << WSAGetLastError() << endl;
        WSACleanup();
        return 0;
    }

    // binding the socket to address and port
    sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_address.sin_port = htons(port);

    char buffer[200];
    printf("Enter your filename: ");
    cin.getline(buffer, 200);

    // sending filename to server
    int byte_sent = sendto(client_socket, (const char*)buffer, strlen(buffer), 0, (struct sockaddr*)&server_address, sizeof(server_address));

    if (byte_sent == -1) {
        printf("Error transmitting data. Terminating.\n");
        WSACleanup();
        return 0;
    }
    else {
        printf("Request sent to download file: %s\n\n", buffer);
    }

    ofstream outputFile;
    outputFile.open(buffer, std::ios_base::binary);

    char receive_data[PACKET_SIZE];

    sockaddr_in server_address2;
    int server_address_length = (int)sizeof(server_address2);
    
    // receiving the file size
    int  byte_received = recvfrom(client_socket, (char *)&receive_data, sizeof(int), 0, (struct sockaddr*)&server_address2, &server_address_length);

    int file_size = ((int * ) receive_data)[0];
    cout << "File size to be received: " << file_size << " bytes\n";
    int total_packets = (file_size / PACKET_SIZE)+1;
    cout << "Total packets to be received: " << total_packets << "\n\n";

    auto start = std::chrono::high_resolution_clock::now();

    Packet packet;
    int cur_packet = 1;

    while(cur_packet<=total_packets){

        byte_received = recvfrom(client_socket, (char *)&packet, sizeof(Packet), 0, (struct sockaddr*)&server_address2, &server_address_length);

        if (byte_received < 0) {
            if (DEBUG) printf("Cound not receive datagram from server.\n");
        }
        else {
            if (cur_packet == total_packets) {
                outputFile.write(packet.packet, file_size%PACKET_SIZE);
                if (DEBUG) cout << "Received packet # " << packet.number << ". [writting bytes to file: " << file_size%PACKET_SIZE << "]\n";
            } 
            else {
                outputFile.write(packet.packet, PACKET_SIZE);
                if (DEBUG) cout << "Received packet # " << packet.number << ". [writting bytes to file: " << PACKET_SIZE << "]\n";
            }
        }

        //std::this_thread::sleep_for(std::chrono::milliseconds(2000));

        // sending ack of above received packet
        int packet_ack[1];
        packet_ack[0] = packet.number;
        byte_sent = sendto(client_socket, (const char*)&packet_ack, sizeof(int), 0, (struct sockaddr*)&server_address, sizeof(server_address));

        if (byte_sent == -1) {
            if (DEBUG) printf("Error sending ack for packe # %d.\n", packet_ack[0]);
        }
        else {
            if (DEBUG) printf("Acknowledgement sent for: %d\n\n", packet_ack[0]);
        }

        cur_packet++;
    }

    outputFile.close();

    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> duration = end - start;
    std::cout << "Time to download file [" << (float)file_size/1024/1023 << " MB] is " << duration.count() << " seconds." << std::endl;

    system("pause");
    WSACleanup();

    return 0;
}
