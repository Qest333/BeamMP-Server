///
/// Created by Anonymous275 on 8/1/2020
///
#include "Lua/LuaSystem.hpp"
#include "Security/Enc.h"
#include "Client.hpp"
#include "Settings.h"
#include "Network.h"
#include "Logger.h"

int FC(const std::string& s,const std::string& p,int n) {
    auto i = s.find(p);
    int j;
    for (j = 1; j < n && i != std::string::npos; ++j){
        i = s.find(p, i+1);
    }
    if (j == n)return int(i);
    else return -1;
}
void Apply(Client*c,int VID,const std::string& pckt){
    std::string Packet = pckt;
    std::string VD = c->GetCarData(VID);
    Packet = Packet.substr(FC(Packet, ",", 2) + 1);
    Packet = VD.substr(0, FC(VD, ",", 2) + 1) +
             Packet.substr(0, Packet.find_last_of('"') + 1) +
             VD.substr(FC(VD, ",\"", 7));
    c->SetCarData(VID, Packet);
}

void VehicleParser(Client*c,const std::string& Pckt){
    if(c == nullptr || Pckt.length() < 4)return;
    std::string Packet = Pckt;
    char Code = Packet.at(1);
    int PID = -1;
    int VID = -1;
    std::string Data = Packet.substr(3),pid,vid;
    switch(Code){ //Spawned Destroyed Switched/Moved NotFound Reset
        case 's':
            if(Data.at(0) == '0'){
                int CarID = c->GetOpenCarID();
                debug(c->GetName() + Sec(" created a car with ID ") + std::to_string(CarID));
                Packet = "Os:"+c->GetRole()+":"+c->GetName()+":"+std::to_string(c->GetID())+"-"+std::to_string(CarID)+Packet.substr(4);
                if(c->GetCarCount() >= MaxCars ||
                   TriggerLuaEvent(Sec("onVehicleSpawn"),false,nullptr,
                                   new LuaArg{{c->GetID(),CarID,Packet.substr(3)}})){
                    Respond(c,Packet,true);
                    std::string Destroy = "Od:" + std::to_string(c->GetID())+"-"+std::to_string(CarID);
                    Respond(c,Destroy,true);
                    debug(c->GetName() + Sec(" (force : car limit/lua) removed ID ") + std::to_string(CarID));
                }else{
                    c->AddNewCar(CarID,Packet);
                    SendToAll(nullptr, Packet,true,true);
                }
            }
            return;
        case 'c':
            pid = Data.substr(0,Data.find('-'));
            vid = Data.substr(Data.find('-')+1,Data.find(':',1)-Data.find('-')-1);
            if(pid.find_first_not_of("0123456789") == std::string::npos && vid.find_first_not_of("0123456789") == std::string::npos){
                PID = stoi(pid);
                VID = stoi(vid);
            }
            if(PID != -1 && VID != -1 && PID == c->GetID()){
                if(!TriggerLuaEvent(Sec("onVehicleEdited"),false,nullptr,
                                    new LuaArg{{c->GetID(),VID,Packet.substr(3)}})) {
                    SendToAll(c, Packet, false, true);
                    Apply(c,VID,Packet);
                }else{
                    std::string Destroy = "Od:" + std::to_string(c->GetID())+"-"+std::to_string(VID);
                    Respond(c,Destroy,true);
                    c->DeleteCar(VID);
                }
            }
            return;
        case 'd':
            pid = Data.substr(0,Data.find('-'));
            vid = Data.substr(Data.find('-')+1);
            if(pid.find_first_not_of("0123456789") == std::string::npos && vid.find_first_not_of("0123456789") == std::string::npos){
                PID = stoi(pid);
                VID = stoi(vid);
            }
            if(PID != -1 && VID != -1 && PID == c->GetID()){
                SendToAll(nullptr,Packet,true,true);
                TriggerLuaEvent(Sec("onVehicleDeleted"),false,nullptr,
                                new LuaArg{{c->GetID(),VID}});
                c->DeleteCar(VID);
                debug(c->GetName() + Sec(" deleted car with ID ") + std::to_string(VID));
            }
            return;
        case 'r':
            SendToAll(c,Packet,false,true);
            return;
        default:
            return;
    }
}
void SyncClient(Client*c){
    if(c->isSynced)return;
    Respond(c,Sec("Sn")+c->GetName(),true);
    SendToAll(c,Sec("JWelcome ")+c->GetName()+"!",false,true);
    TriggerLuaEvent(Sec("onPlayerJoin"),false,nullptr,new LuaArg{{c->GetID()}});
    for (Client*client : CI->Clients) {
        if(client != nullptr){
            if (client != c) {
                for (VData *v : client->GetAllCars()) {
                    Respond(c, v->Data, true);
                }
            }
        }
    }
    c->isSynced = true;
    info(c->GetName() + Sec(" is now synced!"));
}
void ParseVeh(Client*c, const std::string&Packet){
    __try{
            VehicleParser(c,Packet);
    }__except(Handle(GetExceptionInformation(),Sec("Vehicle Handler"))){}
}

void GlobalParser(Client*c, const std::string& Packet){
    static int lastRecv = 0;
    if(Packet.empty() || c == nullptr)return;
    std::string pct;
    char Code = Packet.at(0);

    //V to Z
    if(Code <= 90 && Code >= 86){
        PPS++;
        SendToAll(c,Packet,false,false);
        return;
    }

    switch (Code) {
        case 'P':
            Respond(c, Sec("P") + std::to_string(c->GetID()),true);
            SyncClient(c);
            return;
        case 'p':
            Respond(c,Sec("p"),false);
            UpdatePlayers();
            return;
        case 'O':
            if(Packet.length() > 1000) {
                debug(Sec("Received data from: ") + c->GetName() + Sec(" Size: ") + std::to_string(Packet.length()));
            }
            ParseVeh(c,Packet);
            return;
        case 'J':
            SendToAll(c,Packet,false,true);
            return;
        case 'C':
            if(Packet.length() < 4 || Packet.find(':', 3) == -1)break;
            pct = "C:" + c->GetName() + Packet.substr(Packet.find(':', 3));
            if (TriggerLuaEvent(Sec("onChatMessage"), false, nullptr,
                                new LuaArg{{c->GetID(), c->GetName(), pct.substr(pct.find(':', 3) + 1)}}))break;
            SendToAll(nullptr, pct, true, true);
            pct.clear();
            return;
        case 'E':
            SendToAll(nullptr,Packet,true,true);
            return;
        default:
            return;
    }
}

void GParser(Client*c, const std::string&Packet){
    __try{
            GlobalParser(c, Packet);
    }__except(Handle(GetExceptionInformation(),Sec("Global Handler"))){}
}