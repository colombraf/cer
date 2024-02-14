/*
 * Copyright (C) 2006-2020 Istituto Italiano di Tecnologia (IIT)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "idleManager.h"

YARP_LOG_COMPONENT(IDLE_MANAGER, "r1_obr.idleManager")

IdleManager::IdleManager() 
{
    m_period = 0.5;
    m_rpc_port_name = "/idleManager/rpc"; 
    m_orchestrator_rpc_port_name = "/r1Obr-orchestrator/rpc";
    m_idle_to_orchestrator_rpc_port_name = "/idleManager/orchestrator:rpc";
}

bool IdleManager::configure(ResourceFinder &rf)
{
    if(rf.check("period")) {m_period = rf.find("period").asFloat32();}
    if(rf.check("rpc_port")) {m_rpc_port_name = rf.find("rpc_port").asString();}
    if(rf.check("orchestrator_rpc_port")) {m_orchestrator_rpc_port_name = rf.find("orchestrator_rpc_port").asString();}
    if(rf.check("idle_to_orchestrator_rpc_port")) {m_idle_to_orchestrator_rpc_port_name = rf.find("idle_to_orchestrator_rpc_port").asString();}

    // ---------Open RPC Server Port --------- //
    if (!m_rpc_port.open(m_rpc_port_name))
    {
        yCError(IDLE_MANAGER, "open() error could not open rpc port %s, check network", m_rpc_port_name.c_str());
        return false;
    }
    if (!attach(m_rpc_port))
    {
        yCError(IDLE_MANAGER, "attach() error with rpc port %s", m_rpc_port_name.c_str());
        return false;
    }

    // --------- Open RPC port to orchestrator --------- //
    if(!m_idle_to_orchestrator_rpc_port.open(m_idle_to_orchestrator_rpc_port_name))
    {
        yCError(IDLE_MANAGER, "Unable to open IdleManager RPC port to orchestrator");
        return false;
    }

    // --------- IdleMotions config --------- //
    m_motions = new IdleMotions(rf);
    if(!m_motions->start())
    {
        yCError(IDLE_MANAGER,"IdleMotions configuration failed");
        return false;
    }

    return true;

}


bool IdleManager::close()
{
    if (m_rpc_port.asPort().isOpen())
        m_rpc_port.close();
    
    if (m_idle_to_orchestrator_rpc_port.asPort().isOpen())
        m_idle_to_orchestrator_rpc_port.close(); 

    return true;
}


bool IdleManager::updateModule()
{    
    if (Network::exists(m_orchestrator_rpc_port_name)) 
    {
        if (!Network::isConnected(m_idle_to_orchestrator_rpc_port.getName(), m_orchestrator_rpc_port_name))
        {
            if (!Network::connect(m_idle_to_orchestrator_rpc_port.getName(), m_orchestrator_rpc_port_name)) 
            {
                m_motions->dontMove();
                return true;
            }
        }

        Bottle req{"status"}, rep;
        m_idle_to_orchestrator_rpc_port.write(req,rep);
        if (rep.get(0).asString() != "idle")
            m_motions->dontMove();
        else
            m_motions->nowYouCanMove();
            
    }
    
    
    return true;
}


double IdleManager::getPeriod()
{
    return m_period;
}

bool IdleManager::respond(const Bottle &request, Bottle &reply)
{
    yCInfo(IDLE_MANAGER,"Received: %s",request.toString().c_str());
    
    reply.clear();
    string cmd=request.get(0).asString();
    if (cmd=="help")
    {
        reply.addVocab32("many");
        reply.addString("help : gets this list");
        reply.addString("move : executes a random motion");
        reply.addString("move <int>: executes the motion defined");
        reply.addString("dontMove : the robot cannot execute motions autonomously");
        reply.addString("okMove : the robot can execute motions autonomously");
    }
    else if (cmd=="dontMove")
    {
        m_motions->dontMove();
    }
    else if (cmd=="okMove")
    {
        m_motions->nowYouCanMove();
    }
    else if (cmd=="move")
    {
        int mot=-1;
        if(request.size()>1)
            mot = request.get(1).asInt16();
        
        if(!m_motions->doMotion(mot))
        {
            reply.addString("Error");
            return false;
        }
    }
    else
    {
        reply.addVocab32(Vocab32::encode("nack")); 
        yCWarning(IDLE_MANAGER,"Wrong RPC command. Type 'help'");
    }

    if (reply.size()==0)
        reply.addVocab32(Vocab32::encode("ack")); 

    return true;
}