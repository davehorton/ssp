//
//  ssp-config.cpp
//  ssp
//
//  Created by Dave Horton on 11/16/12.
//  Copyright (c) 2012 Beachdog Networks. All rights reserved.
//

#include <iostream>
#include <fstream>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/exceptions.hpp>
#include <boost/foreach.hpp>
#include <boost/unordered_set.hpp>
#include <boost/unordered_map.hpp>

#include "ssp-config.h"

#include "ssp-controller.h"

using namespace std ;
using boost::property_tree::ptree;
using boost::property_tree::ptree_error;

namespace ssp {

    class Dnis_t {
    public:
        Dnis_t( const string& dnis ) : m_npa(0), m_nxx(0), m_xxxx(0), m_count(1) {
            init(dnis) ;
        }
        Dnis_t( const string& firstDnis, const string& lastDnis) : m_npa(0), m_nxx(0), m_xxxx(0), m_count(1) {
            init(firstDnis);
            
            Dnis_t last( lastDnis ) ;
            if( last.getXxxx() > m_xxxx ) {
                m_count = last.getXxxx() - m_xxxx + 1 ;
            }
        }
        
        /* needed to be able to live in a boost unordered container */
        bool operator==(const Dnis_t& other ) const {
            return (m_npa == other.m_npa && m_nxx == other.m_nxx && m_xxxx == other.m_xxxx) ;
        }
        
        bool matchesOrContains( const Dnis_t& other ) const {
            if( m_npa == other.m_npa && m_nxx == other.m_nxx ) {
                if( m_xxxx == other.m_xxxx || ( m_xxxx < other.m_xxxx && m_xxxx + m_count > other.m_xxxx ) ) {
                    return true ;
                }
            }
            return false ;
        }
        
        unsigned int getNpa() const { return m_npa ; }
        unsigned int getNxx() const { return m_nxx ; }
        unsigned int getXxxx() const { return m_xxxx ; }
        unsigned int getCount() const { return m_count; }
        
    private:
        Dnis_t() ;
        
        void init( const string& dnis ) {
            string npa, nxx, xxxx ;
            size_t len = dnis.length() ;
            try {
                xxxx = dnis.substr( len - 4 ) ;
                nxx = dnis.substr( len - 7, 3) ;
                npa = dnis.substr( len - 10, 3 ) ;
            } catch( const out_of_range& e ) {}
            if( npa.length() > 0 ) m_npa = ::atoi( npa.c_str() ) ;
            if( nxx.length() > 0 ) m_nxx = ::atoi( nxx.c_str() ) ;
            if( xxxx.length() > 0 )m_xxxx = ::atoi( xxxx.c_str() ) ;
        }
        
        unsigned int m_npa ;
        unsigned int m_nxx ;
        unsigned int m_xxxx ;
        unsigned int m_count ;
    } ;
    /* needed to be able to live in a boost unordered container */
    size_t hash_value( const Dnis_t& dnis) {
        std::size_t seed = 0;
        boost::hash_combine(seed, dnis.getNpa());
        boost::hash_combine(seed, dnis.getNxx());
        boost::hash_combine(seed, dnis.getXxxx());
        return seed;
    }
    typedef boost::unordered_map< string, boost::unordered_set<Dnis_t> > CustomerDnisMap_t ;
    

    class CarrierAddressSpace_t {
    public:
        CarrierAddressSpace_t( const string& address, unsigned int port = 5060, const string& netmask = "255.255.255.255") : m_address(address), m_netmask(netmask), m_port(port) {}
        ~CarrierAddressSpace_t() {}
        
        bool operator==( const CarrierAddressSpace_t& other ) const {
            return m_address == other.m_address && m_netmask == other.m_netmask ;
        }
        
        bool matchesOrContains( const string& address ) const {
            if( m_address == address ) {
                return true ;
            }
            return false ;
        }
        
        const string& getAddress() const { return m_address; }
        const string& getNetmask() const { return m_netmask; }
        unsigned int getPort() const { return m_port; }
        
    private:
        CarrierAddressSpace_t() ;
        
        string m_address ;
        string m_netmask ;
        unsigned int m_port ;
    } ;
    /* needed to be able to live in a boost unordered container */
    size_t hash_value( const CarrierAddressSpace_t& c) {
        std::size_t seed = 0;
        boost::hash_combine(seed, c.getAddress());
        boost::hash_combine(seed, c.getNetmask());
        boost::hash_combine(seed, c.getPort());
        return seed;
    }
    typedef boost::unordered_map< string, boost::unordered_set<CarrierAddressSpace_t> > CarrierServerMap_t ;
 
    class Appserver_t {
    public:
        Appserver_t( const string& address, unsigned int port = 5060 ) : m_address( address ), m_port(port) {
        }
        
        bool operator==(const Appserver_t& other) const {
            return m_address == other.m_address && m_port == other.m_port ;
        }
        
        const string& getAddress() const { return m_address ; }
        unsigned int getPort() const { return m_port ; }
        unsigned int getPingInterval() const { return m_pingInterval ; }
        unsigned int getMaxSessions() const { return m_maxSessions; }
        
        Appserver_t& setPingInterval( unsigned int i ) { m_pingInterval = i; return *this; }
        Appserver_t& setMaxSessions( unsigned int i ) { m_maxSessions = i; return *this; }
        
        
    private:
        Appserver_t() ;
        
        string m_name ;
        string m_address ;
        unsigned int m_port ;
        unsigned int m_pingInterval ;
        unsigned int m_maxSessions;
    } ;
    /* needed to be able to live in a boost unordered container */
    size_t hash_value( const Appserver_t& a) {
        std::size_t seed = 0;
        boost::hash_combine(seed, a.getAddress());
        boost::hash_combine(seed, a.getPort());
        return seed;
    }
    typedef boost::unordered_map< string, Appserver_t > AppserverMap_t ;
    
    
    class AppserverGroup_t {
    public:
        AppserverGroup_t() {}
        AppserverGroup_t( const string& name, const boost::unordered_set<Appserver_t>& setAppserver ) : m_name( name ), m_setAppserver(setAppserver) {}
        ~AppserverGroup_t() {}
        
        bool operator==(const AppserverGroup_t& other ) const {
            return m_name == other.m_name ;
        }
        
        AppserverGroup_t& addServer( const Appserver_t& appserver ) { m_setAppserver.insert( appserver ); }
        const string& getName() const { return m_name; }
        const boost::unordered_set<Appserver_t>& getAppservers() const { return m_setAppserver; }
        
    private:
        
        string m_name ;
        boost::unordered_set<Appserver_t> m_setAppserver ;
    } ;
    /* needed to be able to live in a boost unordered container */
    size_t hash_value( const AppserverGroup_t& g) {
        std::size_t seed = 0;
        boost::hash_combine(seed, g.getName());
        return seed;
    }
    typedef boost::unordered_map< string, AppserverGroup_t > AppserverGroupMap_t ;
    
    class Routing_t {
    public:
        class Route_t {
        public:
            Route_t() :m_bIsValid(false) {}
            /*
            Route_t( route_selector sel, const string& target, routing_strategy strat ) : m_selector(sel), m_target(target), m_strategy(strat)  {
            
                m_bIsValid = true ;
            }
             */
            Route_t( const string& sel, const string& target, const string& strategy, const boost::unordered_set<string>& values ) : m_setValue(values) {
                m_strategy = String2RoutingStrategy( strategy ) ;
                if( unknown_routing_strategy == m_strategy ) {
                    return ;
                }
                
                m_selector = String2RouteSelector( sel ) ;
                if( unknown_route_selector == m_selector ) {
                    return ;
                }
            
                m_bIsValid = true; 
            }
            ~Route_t() {}
            bool operator==(const Route_t& other ) const {
                return m_selector == other.m_selector && m_target == other.m_target && m_strategy == other.m_strategy && m_setValue == other.m_setValue ;
            }
            bool isValid() { return m_bIsValid ; }
            
            route_selector getRouteSelector() const { return m_selector ;}
            const string& getTarget() const { return m_target ; }
            routing_strategy getRoutingStrategy() const { return m_strategy; }
            const boost::unordered_set<string> getValues() const { return m_setValue; }
            
        private:
            route_selector m_selector ;
            string m_target ;
            routing_strategy m_strategy ;
            boost::unordered_set<string> m_setValue ;
            bool m_bIsValid ;
            
        } ;
       
        Routing_t() : m_inboundStrategy(round_robin), m_outboundStrategy(same_in_same_out_server) {} ;
        Routing_t( routing_strategy ib, routing_strategy ob, const AppserverGroup_t& ibGroup, const string& obTarget ) : m_inboundStrategy(ib), m_outboundStrategy(ob),
            m_inboundGroup(ibGroup), m_outboundTarget(obTarget) {}
        ~Routing_t() {}
        
        routing_strategy getInboundRoutingStrategy() const { return m_inboundStrategy ; }
        routing_strategy getOutboundRoutingStrategy() const { return m_outboundStrategy ; }
        const AppserverGroup_t& getInboundAppserverGroup() const { return m_inboundGroup; }
        const string& getOutboundTarget() const { return m_outboundTarget; }

        bool getInboundRouteForSelector( route_selector r, const string& strValue, Route_t& route )  const {
            boost::unordered_map< route_selector, boost::unordered_set< Route_t > >::const_iterator it = m_mapInboundRoutes.find( r ) ;
            if( m_mapInboundRoutes.end() == it ) return false ; //no routes for that selector
            
            /* check if our value (ani/dnis) is in the set for this route */
            BOOST_FOREACH( const Route_t& r, it->second ) {
                boost::unordered_set<string>::const_iterator itValues = route.getValues().find( strValue ) ;
                if( r.getValues().end() != itValues ) {
                    route = r;
                    return true ;
                }
            }
            return false ;
        }
        
        bool getOutboundRouteForSelector( route_selector r, const string& strValue, Route_t& route )  const {
            boost::unordered_map< route_selector, boost::unordered_set< Route_t > >::const_iterator it = m_mapOutboundRoutes.find( r ) ;
            if( m_mapOutboundRoutes.end() == it ) return false ; //no routes for that selector
            
            /* check if our value (ani/dnis) is in the set for this route */
            BOOST_FOREACH( const Route_t& r, it->second ) {
                boost::unordered_set<string>::const_iterator itValues = route.getValues().find( strValue ) ;
                if( r.getValues().end() != itValues ) {
                    route = r;
                    return true ;
                }
            }
            return false ;
        }
        
        Routing_t& setInboundStrategy( routing_strategy strat ) { m_inboundStrategy = strat; return *this; }
        Routing_t& setOutboundStrategy(routing_strategy strat ) { m_outboundStrategy = strat; return *this; }
        Routing_t& setInboundAppserverGroup( const AppserverGroup_t group ) { m_inboundGroup = group; return *this; }
        Routing_t& setOutboundTarget( const string& target) { m_outboundTarget = target; return *this; }
        Routing_t& addInboundRoute( const Route_t& r ) { m_mapInboundRoutes[r.getRouteSelector()].insert( r ) ; return *this ; }
        Routing_t& addOutboundRoute( const Route_t& r ) { m_mapOutboundRoutes[r.getRouteSelector()].insert( r ) ; return *this ; }
                
         
    private:
        
        routing_strategy m_inboundStrategy ; ;
        AppserverGroup_t m_inboundGroup ;
        routing_strategy m_outboundStrategy;
        string m_outboundTarget ;
        
        boost::unordered_map< route_selector, boost::unordered_set< Route_t > > m_mapInboundRoutes ;
        boost::unordered_map< route_selector, boost::unordered_set< Route_t > > m_mapOutboundRoutes ;        
        
    } ;
    /* needed to be able to live in a boost unordered container */
    size_t hash_value( const Routing_t::Route_t& r) {
        std::size_t seed = 0;
        
        /* hash the selector, and the first value -- since values (ie phone numbers, customer names) can only live in one container, each will be unique in that container */
        boost::hash_combine(seed, r.getRouteSelector() ) ;
        if( r.getValues().size() > 0 ) boost::hash_combine( seed, *r.getValues().begin() ) ;
        return seed;
    }

    
    /**
     Internal implementation class
    */
    class SspConfig::Impl {
    public:
        Impl( const char* szFilename) : m_bIsValid(false) {
            cout << "reading configuration file: " << szFilename << endl ;
            try {
                std::filebuf fb;
                if( !fb.open (szFilename,std::ios::in) ) {
                    cerr << "Unable to open configuration file: " << szFilename << endl ;
                    return ;
                }
                std::istream is(&fb);
                
                ptree pt;
                read_xml(is, pt);
                
                /* logging configuration  */
                m_syslogAddress = pt.get<string>("ssp.logging.syslog.address", "localhost") ;
                m_sysLogPort = pt.get<unsigned int>("ssp.logging.syslog.address", 516) ;
                m_syslogFacility = pt.get<string>("ssp.logging.syslog.facility","local7") ;
                
                m_sipAddress = pt.get<string>("ssp.sip.address", "*") ;
                m_sipPort = pt.get<unsigned int>("ssp.sip.port", 5060) ;
                
                string ibStrategy = pt.get<string>("ssp.routing.inbound.<xmlattr>.strategy", "") ;
                string ibTarget = pt.get<string>("ssp.routing.inbound.<xmlattr>.target", "") ;
                string obStrategy = pt.get<string>("ssp.routing.outbound.<xmlattr>.strategy", "") ;
                string obTarget = pt.get<string>("ssp.routing.outbound.<xmlattr>.route", "") ;
                
                if( ibTarget.empty() ) {
                    cerr << "Configuration error: no default inbound routing target was provided" ;
                    return ;
                }
                
                AppserverMap_t mapAppserver;
                BOOST_FOREACH( ptree::value_type const& v, pt.get_child("ssp") ) {

                    /* customer/dnis configuration */
                    if( v.first == "dnis-list" ) {
                        
                        /* get name */
                        string customerName ;
                        if( !getXmlAttribute( v, "name", customerName ) )
                            return ;
                        
                        boost::unordered_set<Dnis_t> setDnis ;
                        
                        /* iterate child nodes: dnis and dnis-block */
                        BOOST_FOREACH( ptree::value_type const& v2, v.second ) {
                            if( v2.first == "dnis" ) {
                                Dnis_t dnis( v2.second.data() ) ;
                                setDnis.insert( dnis ) ;
                            }
                            else if( v2.first == "dnis-block" ) {
                                string first = v2.second.get<string>("first") ;
                                string last = v2.second.get<string>("last") ;
                                Dnis_t dnis( first,last ) ;
                                setDnis.insert( dnis ) ;
                            }
                        }
                        pair<CustomerDnisMap_t::iterator, bool > ret = m_mapCustomerDnis.insert( CustomerDnisMap_t::value_type( customerName, setDnis ) ) ;
                        if( !ret.second ) {
                            cerr << "Multiple dnis-list elements have duplicate name attribute values" << endl ;
                            return ;
                        }
                    }
                    else if( v.first == "carrier" ) {
                        /* get name */
                        string carrierName ;
                        if( !getXmlAttribute( v, "name", carrierName ) )
                            return ;

                        boost::unordered_set<CarrierAddressSpace_t> setInbound ;
                        boost::unordered_set<CarrierAddressSpace_t> setOutbound ;
                        BOOST_FOREACH( ptree::value_type const& v2, v.second.get_child("inbound") ) {
                            if( v2.first == "address") {
                                CarrierAddressSpace_t c( v2.second.data(), v2.second.get("<xmlattr>.port", 5060), v2.second.get("<xmlattr>.netmask", "255.255.255.255") ) ;
                                setInbound.insert( c ) ;
                            }
                        }
                        BOOST_FOREACH( ptree::value_type const& v2, v.second.get_child("outbound") ) {
                            CarrierAddressSpace_t c( v2.second.data(), v2.second.get("<xmlattr>.port", 5060), v2.second.get("<xmlattr>.netmask", "255.255.255.255") ) ;
                            setOutbound.insert( c ) ;
                        }
                        
                        pair<CarrierServerMap_t::iterator, bool> ret = m_mapInboundCarrier.insert( CarrierServerMap_t::value_type( carrierName, setInbound ) ) ;
                        if( !ret.second ) {
                            cerr << "Multiple carrier elements have duplicate name attribute values" << endl ;
                            return ;
                        }
                    }
                    else if( v.first == "appserver" ) {
                        /* get name */
                        string appserverName ;
                        string address = v.second.data()  ;
                        if( !getXmlAttribute( v, "name", appserverName ) ) {
                            appserverName = address ;
                        }

                        
                        Appserver_t as( v.second.data(), v.second.get("port", 5060) ) ;
                        try {
                            unsigned int pingInterval = v.second.get<unsigned int>("ping-interval") ;
                            as.setPingInterval( pingInterval ) ;
                        } catch( const ptree_error& err ) {}
                        try {
                            unsigned int maxSessions = v.second.get<unsigned int>("max-sessions") ;
                            as.setMaxSessions( maxSessions ) ;
                        } catch( const ptree_error& err ) {}
                        
                        pair<AppserverMap_t::iterator, bool> ret = mapAppserver.insert( AppserverMap_t::value_type( appserverName, as ) ) ;
                        if( !ret.second ) {
                            cerr << "Multiple appserver elements have duplicate name attribute values or ip addresses" << endl ;
                            return ;
                        }
                    }
                    else if( v.first == "appserver-group" ) {
                        /* get name */
                        string appserverGroupName ;
                        if( !getXmlAttribute( v, "name", appserverGroupName ) ) {
                            return ;
                        }
                        
                        boost::unordered_set<Appserver_t> setAppserver ;
                        
                         BOOST_FOREACH( ptree::value_type const& v2, v.second ) {
                             if( v2.first == "appserver") {
                                string asName = v2.second.data() ;
                                AppserverMap_t::iterator it = mapAppserver.find( asName )  ;
                                if( mapAppserver.end() == it ) {
                                    cerr << "Unable to find appserver element with name: " << asName << endl ;
                                    return ;
                                }
                                setAppserver.insert( it->second ) ;
                             }
                        }
                        
                        AppserverGroup_t asg( appserverGroupName, setAppserver ) ;
                        pair<AppserverGroupMap_t::iterator, bool> ret = m_mapAppserverGroup.insert( AppserverGroupMap_t::value_type( appserverGroupName, asg ) ) ;
                        if( !ret.second ) {
                            cerr << "Multiple appserver-group elements have duplicate name attribute values" << endl ;
                            return ;
                        }
                    }
                    else if( v.first == "routing" ) {
                        BOOST_FOREACH( const ptree::value_type& v2, v.second.get_child("inbound") ) {
                            if( v2.first == "route") {
                                string selector = v2.second.get("<xmlattr>.selector","") ;
                                string target = v2.second.get("<xmlattr>.target", ibTarget) ;
                                string strategy = v2.second.get("<xmlattr>.strategy", ibStrategy) ;
                                boost::unordered_set<string> setValues ;
                                BOOST_FOREACH( const ptree::value_type& v3, v2.second ) {
                                    if( v3.first == "item") setValues.insert( v3.second.data() ) ;
                                }
                                
                                if( setValues.empty() ) {
                                    cerr << "You must supply at least one <item/> child of the <inbound/> element" << endl ;
                                    return ;
                                }
                                
                                Routing_t::Route_t r( selector, target, strategy, setValues ) ;
                                if( !r.isValid() ) {
                                    return ;
                                }
                                m_routing.addInboundRoute( r ) ;
                                
                            }
                        }
                        BOOST_FOREACH( const ptree::value_type& v2, v.second.get_child("outbound") ) {
                            if( v2.first == "route") {
                                string selector = v2.second.get("<xmlattr>.selector","") ;
                                string target = v2.second.get("<xmlattr>.target", ibTarget) ;
                                string strategy = v2.second.get("<xmlattr>.strategy", ibStrategy) ;
                                boost::unordered_set<string> setValues ;
                                BOOST_FOREACH( const ptree::value_type& v3, v2.second ) {
                                    if( v3.first == "item") setValues.insert( v3.second.data() ) ;
                                }
                                
                                if( setValues.empty() ) {
                                    cout << "You must supply at least one <item/> child of the <outbound/> element" << endl ;
                                    return ;
                                }
                                
                                Routing_t::Route_t r( selector, target, strategy, setValues ) ;
                                if( !r.isValid() ) {
                                    return ;
                                }
                                m_routing.addOutboundRoute( r ) ;
                                
                            }
                        }
                    }
                }
                fb.close() ;
                
                /* resolve default inbound target */
                AppserverGroup_t group ;
                if( !getAppserverGroup( ibTarget, group ) ) {
                    cerr << "Default inbound appserver group not found: " << ibTarget << endl ;
                    return ;
                }
                m_routing.setInboundAppserverGroup( group ) ;
                
                m_bIsValid = true ;
            } catch( exception& e ) {
                cerr << "Error reading configuration file: " << e.what() << endl ;
            }    
        }
        ~Impl() {
        }
                   
        bool isValid() const { return m_bIsValid; }
        const string& getSyslogAddress() const { return m_syslogAddress; }
        unsigned int getSyslogPort() const { return m_sysLogPort ; }
        
        const string& getSipAddress() const { return m_sipAddress; }
        unsigned int getSipPort() const { return m_sipPort ; }
        
        bool getSyslogTarget( std::string& address, unsigned int& port ) const {
            address = m_syslogAddress ;
            port = m_sysLogPort  ;
            return true ;
        }
        bool getCustomer( const std::string& dnis, std::string& customer) const {
            for( CustomerDnisMap_t::const_iterator it = m_mapCustomerDnis.begin(); it != m_mapCustomerDnis.end(); it++ ) {
                BOOST_FOREACH( const Dnis_t& d, it->second ) {
                    if( d.matchesOrContains( dnis ) ) {
                        customer = it->first ;
                        return true ;
                    }
                }
            }
            return false ;
        }
        bool getCarrier( const std::string& sourceAddress, std::string& carrier) const {
            for( CarrierServerMap_t::const_iterator it = m_mapInboundCarrier.begin(); it != m_mapInboundCarrier.end(); it++ ) {
                BOOST_FOREACH( const CarrierAddressSpace_t& c, it->second ) {
                    if( c.matchesOrContains( sourceAddress ) ) {
                        carrier = it->first ;
                        return true ;
                    }
                }
            }
            return false ;
        }
        bool getInboundRoutes( const std::string& sourceAddress, const std::string& dnis, const std::string& ani, std::vector<std::string>& routes, routing_strategy& strategy, routing_error& error ) const {
            Routing_t::Route_t route ;
            string carrierName, customerName ;
            if( !getCarrier( sourceAddress, carrierName ) ) {
                error = invalid_sender ;
                return false ;
            }
            if( !getCustomer( dnis, customerName ) ) {
                error = dnis_not_provisioned ;
                return false ;
            }
            
            
            /* ani-based routing takes precedence, then customer/dnis-based routing */
            AppserverGroup_t group ;
            if( m_routing.getInboundRouteForSelector( ani_selector, ani, route ) || m_routing.getInboundRouteForSelector( customer_selector, customerName, route ) ) {
                
                string s = route.getTarget()  ;
                strategy = route.getRoutingStrategy() ;
                AppserverGroupMap_t::const_iterator it = m_mapAppserverGroup.find( s ) ;
                if( m_mapAppserverGroup.end() == it ) {
                    error = no_routes_provisioned ;
                    return false ;
                }
                group = it->second ;
            }
            
             /* default routing */
            else {
                group = m_routing.getInboundAppserverGroup() ;
                strategy = m_routing.getInboundRoutingStrategy() ;
            }
            
            /* resolve appserver group into a set of addresses */
            BOOST_FOREACH( const Appserver_t& as, group.getAppservers() ) {
                routes.push_back( as.getAddress() ) ;
            }
            return true ;

        }
        bool getOutboundRoutes( const std::string& dnis, const std::string& ani, std::vector<std::string>& routes, routing_strategy& strategy, routing_error& error ) const {
            Routing_t::Route_t route ;
            string customerName ;
            if( !getCustomer( dnis, customerName ) ) {
                error = dnis_not_provisioned ;
                return false ;
            }
            
            
            /* ani-based routing takes precedence, then customer/dnis-based routing */
            string carrierName  ;
            if( m_routing.getOutboundRouteForSelector( ani_selector, ani, route ) || m_routing.getOutboundRouteForSelector( customer_selector, customerName, route ) ) {
                
                carrierName = route.getTarget()  ;
                strategy = route.getRoutingStrategy() ;
            }
            
            /* default routing */
            else {
                carrierName = m_routing.getOutboundTarget();  ;
                strategy = m_routing.getOutboundRoutingStrategy() ;
            }
            
            /* resolve carrier name into a set of server addresses */
            CarrierServerMap_t::const_iterator it = m_mapOutboundCarrier.find( carrierName ) ;
            if( m_mapOutboundCarrier.end() == it ) {
                error = no_routes_provisioned ;
                return false ;
            }
            
            BOOST_FOREACH( const CarrierAddressSpace_t& c, it->second ) {
                routes.push_back( c.getAddress() ) ;
            }
            return true ;
            
        }
        
        bool getSyslogFacility( sinks::syslog::facility& facility ) const {
        
	    std::cout << "syslog facility " << m_syslogFacility << endl ;

            if( m_syslogFacility.empty() ) return false ;
            
            if( m_syslogFacility == "local0" ) facility = sinks::syslog::local0 ;
            else if( m_syslogFacility == "local1" ) facility = sinks::syslog::local1 ;
            else if( m_syslogFacility == "local2" ) facility = sinks::syslog::local2 ;
            else if( m_syslogFacility == "local3" ) facility = sinks::syslog::local3 ;
            else if( m_syslogFacility == "local4" ) facility = sinks::syslog::local4 ;
            else if( m_syslogFacility == "local5" ) facility = sinks::syslog::local5 ;
            else if( m_syslogFacility == "local6" ) facility = sinks::syslog::local6 ;
            else if( m_syslogFacility == "local7" ) facility = sinks::syslog::local7 ;
            else return false ;
            
            return true ;
        }
        
        peer_type queryPeerType( const string& host ) {
            CarrierAddressSpace_t c( host ) ;
            
            /* check carrier hosts first */
            for( boost::unordered_map< string, boost::unordered_set<CarrierAddressSpace_t> >::const_iterator it = m_mapInboundCarrier.begin();
                it != m_mapInboundCarrier.end(); it++ ) {
                
                boost::unordered_set<CarrierAddressSpace_t> set = it->second ;
                if( set.end() != set.find( c ) ) {
                    return external_peer ;
                }
            }
            
            /* now check appservers */
            Appserver_t as( host ) ;
            for( boost::unordered_map< string, AppserverGroup_t>::const_iterator it = m_mapAppserverGroup.begin();
                it != m_mapAppserverGroup.end(); it++ ) {
                
                AppserverGroup_t group = it->second ;
                if( group.getAppservers().end() != group.getAppservers().find( as ) ) {
                    return internal_peer ;
                }
            }
            
            return unknown_peer ;
        }
        
        bool getAppserverGroup( const string& name, AppserverGroup_t& group ) {
            boost::unordered_map< string, AppserverGroup_t>::const_iterator it = m_mapAppserverGroup.find( name ) ;
            if( m_mapAppserverGroup.end() == it ) return false ;
            
            group = it->second ;
            return true;             
        }

        
    private:
        
        bool getXmlAttribute( ptree::value_type const& v, const string& attrName, string& value ) {
            try {
                string key = "<xmlattr>." ;
                key.append( attrName ) ;
                value = v.second.get<string>( key ) ;
            } catch( const ptree_error& err ) {
                cerr << "missing attribute " << attrName << endl ;
                return false ;
            }
            if( value.empty() ) return false ;
            return true ;
        }
    
        
        
        bool m_bIsValid ;
        string m_syslogAddress ;
        unsigned int m_sysLogPort ;
        string m_syslogFacility ;
        
        string m_sipAddress ;
        unsigned int m_sipPort ;

        CustomerDnisMap_t m_mapCustomerDnis ;
        CarrierServerMap_t m_mapInboundCarrier ;
        CarrierServerMap_t m_mapOutboundCarrier ;
        AppserverGroupMap_t m_mapAppserverGroup ;
        Routing_t m_routing ;
    } ;
    
    /*
     Public interface
    */
    SspConfig::SspConfig( const char* szFilename ) : m_pimpl( new Impl(szFilename) ) {
    }
    
    SspConfig::~SspConfig() {
        
    }
    
    bool SspConfig::isValid() {
        return m_pimpl->isValid() ;
    }
    
    bool SspConfig::getSipAddress( std::string& sipAddress ) const ;
    bool SspConfig::getSipPort( unsigned int& sipPort ) const ;
    
   
    bool SspConfig::getSyslogTarget( std::string& address, unsigned int& port ) const {
        return m_pimpl->getSyslogTarget( address, port ) ;
    }
    bool SspConfig::getCustomer( const std::string& dnis, std::string& customer) const {
        return m_pimpl->getCustomer( dnis, customer ) ;
    }
    bool SspConfig::getCarrier( const std::string& sourceAddress, std::string& customer) const {
        return m_pimpl->getCarrier( sourceAddress, customer ) ;
    }
    bool SspConfig::getInboundRoutes( const std::string& sourceAddress, const std::string& dnis, const std::string& ani, std::vector<std::string>& routes, routing_strategy& strategy, routing_error& error ) const {
        return m_pimpl->getInboundRoutes( sourceAddress, dnis, ani, routes, strategy,  error );
    }
    bool SspConfig::getOutboundRoutes( const std::string& dnis, const std::string& ani, std::vector<std::string>& routes, routing_strategy& strategy, routing_error& error ) const {
        return m_pimpl->getOutboundRoutes( dnis, ani, routes, strategy, error ) ;
    }
    bool SspConfig::getSyslogFacility( sinks::syslog::facility& facility ) const {
        return m_pimpl->getSyslogFacility( facility ) ;
    }
    
    peer_type SspConfig::queryPeerType( const std::string& strAddress ) {
        return m_pimpl->queryPeerType( strAddress ) ;
    }

    void SspConfig::Log() const {
        SSP_LOG(log_notice) << "Configuration:" << endl ;
    }
}
