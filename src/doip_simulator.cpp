#include "doip_simulator.h"

/**
 * Constructor. Creates a DoIPServer for this simulator
 */
DoIPSimulator::DoIPSimulator() {
    curr_diag_data = nullptr;
    doipserver = new DoIPServer();
}

/**
 * Initialize server instance with required callbacks, start the doip server
 */
void DoIPSimulator::start() {
    DiagnosticCallback cb = std::bind(&DoIPSimulator::receiveFromLib, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    DiagnosticMessageNotification dmn = std::bind(&DoIPSimulator::diagMessageReceived, this, std::placeholders::_1);
    doipserver->setCallback(cb, dmn);
    
    //Udp
    doipserver->setupUdpSocket();
    doipserver->receiveUdpMessage();
    
    //Tcp
    doipserver->setupSocket();
    while(1)
        doipserver->receiveMessage();
}

/**
 * Is called when the doip library receives a diagnostic message.
 * @param address   logical address of the ecu
 * @param data      message which was received
 * @param length    length of the message
 */
void DoIPSimulator::receiveFromLib(unsigned char* address, unsigned char* data, int length) {
    printf("CarSimulator DoIP Simulator received: ");
    for(int i = 0; i < length; i++) {
        printf("0x%02X ", data[i]);
    }
    printf(" from doip lib.\n");
    
    int index = findECU(address);
    if(index != -1) {
        std::vector<unsigned char> response = ecus.at(index)->getUdsReceiver()->proceedDoIPData(data,length);
        
        if(response.size() > 0) {
            sendDiag(response);
        }
    }
}

/**
 * Constantly checks if the simulator received data from the lib, if thats the case
 * it passes the received data to the ecu
 */
void DoIPSimulator::processDiagData() {
    while(curr_diag_data == nullptr) {
        sleep(2);
    }
    
    std::vector<unsigned char> response = ecus.at(0)->getUdsReceiver()->proceedDoIPData(curr_diag_data, curr_diag_data_length);
    
    curr_diag_data_length = 0;
    curr_diag_data = nullptr;
    
    if(response.size() > 0) {
        sendDiag(response);
    }
}

/**
 * Passes the received response from a ecu back to the doip library
 * @param data      respone from a ecu that will be send back
 */
void DoIPSimulator::sendDiag(const std::vector<unsigned char> data) {
    
    unsigned char* msg = new unsigned char[data.size()];
    for(unsigned int i = 0; i < data.size(); i++) {
        msg[i] = data[i];
    }
    
    doipserver->receiveDiagnosticPayload(msg, data.size());
    delete[] msg;
}

/**
 * Adds a ECU to the list
 * @param ecu   Pointer to the ecu
 */
void DoIPSimulator::addECU(ElectronicControlUnit* ecu) {
    ecus.push_back(ecu);
}

/**
 * Will be called when the doip library receives a diagnostic message.
 * The library notifies the application about the message.
 * Checks if there is a ecu with the logical address
 * @param targetAddress     logical address to the ecu
 * @return                  If a positive or negative ACK should be send to the client
 */
bool DoIPSimulator::diagMessageReceived(unsigned char* targetAddress) {
    PayloadType ackType;
    unsigned char ackCode;
    
    //if there isnt a ecu with the target address 
    if(findECU(targetAddress) == -1) {
        //send negative ack with unknown target address and return
        ackType = PayloadType::DIAGNOSTICNEGATIVEACK;
        ackCode = 0x03;
        doipserver->sendDiagnosticAck(ackType, ackCode);
        return false;
    }
    
    //positiv ack
    ackType = PayloadType::DIAGNOSTICPOSITIVEACK;
    ackCode = 0x00;
    
    //negative ack
    //ackType = PayloadType::DIAGNOSTICNEGATIVEACK;
    //ackCode = 0x02;
    
    doipserver->sendDiagnosticAck(ackType, ackCode);
    
    if(ackType == PayloadType::DIAGNOSTICPOSITIVEACK) {
        std::cout << "Send positive diagnostic message ack" << std::endl;
        return true;
    } else {
        std::cout << "Send negative diagnostic message ack" << std::endl;
        return false; 
    }       
}

/**
 * Find a ECU where the given address matches with the logical address
 * @param address   target address
 * @return          index of ecu in the vector
 */
int DoIPSimulator::findECU(unsigned char* address) {
    int ecuIndex = -1;

    //Check if there is a running ecu where logicalAddress == targetAddress
    for(unsigned int i = 0; i < ecus.size(); i++) {
        unsigned char logicalAddress [2] = {ecus.at(i)->getLogicalAddress() >> 8, ecus.at(i)->getLogicalAddress() & 0xFF };
        
        if(logicalAddress[0] == address[0] && logicalAddress[1] == address[1]) {
            ecuIndex = i;
            break;
        }
    }
    
    return ecuIndex;
}
