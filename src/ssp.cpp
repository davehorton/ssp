//
//  ssp.cpp
//  ssp
//
//  Created by Dave Horton on 11/28/12.
//  Copyright (c) 2012 Beachdog Networks. All rights reserved.
//

#include "ssp.h"
#include <boost/unordered_map.hpp>
#include <boost/assign/list_of.hpp>


namespace ssp {

    routing_strategy String2RoutingStrategy( const std::string& s) {
        
        static boost::unordered_map<std::string, routing_strategy> map = boost::assign::map_list_of
        (std::string("round-robin"),                round_robin)
        (std::string("least-loaded"),               least_loaded)
        (std::string("random"),                     random)
        (std::string("same-in-same-out-carrier"),   same_in_same_out_carrier)
        (std::string("same-in-same-out-server"),    same_in_same_out_server)
        (std::string("selected-carrier"),           selected_carrier)
        (std::string("selected-server"),            selected_server)
        ;
        
        boost::unordered_map<std::string, routing_strategy>::const_iterator it = map.find(s) ;
        return map.end() == it ? unknown_routing_strategy : it->second ;
    } ;
    
    route_selector String2RouteSelector( const std::string& s) {
        
        static boost::unordered_map<std::string, route_selector> map = boost::assign::map_list_of
        (std::string("ani"),                ani_selector)
        (std::string("customer"),               customer_selector)
        ;
        
        boost::unordered_map<std::string, route_selector>::const_iterator it = map.find(s) ;
        return map.end() == it ? unknown_route_selector : it->second ;
    } ;
    

    const std::string& RoutingError2String( routing_error error ) {
        
        static std::string unknown = "unknown error" ;
        static boost::unordered_map<routing_error , std::string> map = boost::assign::map_list_of
        (dnis_not_provisioned,                std::string("dnis is not provisioned"))
        (invalid_sender,                      std::string("invalid sender"))
        (no_available_server,                 std::string("no available server"))
        (no_routes_provisioned,               std::string("no routes provisioned"))
        ;
        
        boost::unordered_map<routing_error , std::string>::const_iterator it = map.find(error) ;
        return map.end() == it ? unknown : it->second ;
        
        
    }

}