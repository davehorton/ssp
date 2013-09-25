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
        CarrierAddressSpace_t( const string& carrier, const string& address, unsigned int port = 5060, 
            const string& netmask = "255.255.255.255", unsigned int qty = 1) : m_address(address), m_netmask(netmask), m_port(port), m_carrier(carrier), 
            m_qty(qty), m_selectCount(0) {
        }
        ~CarrierAddressSpace_t() {}
        
        bool operator==( const CarrierAddressSpace_t& other ) const {
            return m_address == other.m_address && m_netmask == other.m_netmask && m_port == other.m_port ;
        }
        
        bool matchesOrContains( const string& address, unsigned int port ) const {
            if( m_address == address && m_port == port ) {
                return true ;
            }
            return false ;
        }
        
        const string& getCarrier() const { return m_carrier; }
        const string& getAddress() const { return m_address; }
        const string& getNetmask() const { return m_netmask; }
        unsigned int getPort() const { return m_port; }
        bool haveAdvancedToEnd() {
            if( m_selectCount == m_qty ) {
                m_selectCount = 0 ;
                return true ;
            }
            m_selectCount++ ;
            return false ;
        }
        
    private:
        CarrierAddressSpace_t() ;
        
        string m_carrier ;
        string m_address ;
        string m_netmask ;
        unsigned int m_port ;
        unsigned int m_qty ;
        unsigned int m_selectCount ;
    } ;
    /* needed to be able to live in a boost unordered container */
    size_t hash_value( const CarrierAddressSpace_t& c) {
        std::size_t seed = 0;
        boost::hash_combine(seed, c.getAddress());
        boost::hash_combine(seed, c.getNetmask());
        boost::hash_combine(seed, c.getPort());
        return seed;
    }
    
    typedef boost::unordered_map< string, CarrierAddressSpace_t > CarrierServerMap_t ;

    class TerminationRoute_t {
    public:
        TerminationRoute_t(const vector<CarrierAddressSpace_t>& v, const string& chargeNumber) : m_currentTrunk(0), m_vecTrunks(v), m_chargeNumber(chargeNumber) {}
        ~TerminationRoute_t() {}
        
        void advanceTrunk() {
            if( ++m_currentTrunk >= m_vecTrunks.size() ) m_currentTrunk = 0 ;
        }
        void getNextTrunk( string& trunk, string& chargeNumber ) {
            stringstream s ;
            CarrierAddressSpace_t& c = m_vecTrunks.at( m_currentTrunk ) ;

            if( c.haveAdvancedToEnd() ) {
                this->advanceTrunk() ;
                CarrierAddressSpace_t& d = m_vecTrunks.at( m_currentTrunk ) ;
                d.haveAdvancedToEnd() ;
                s << d.getAddress() ;
                if( 5060 != d.getPort() ) s << ":" << d.getPort() ;
            }
            else {
                s << c.getAddress() ;
                if( 5060 != c.getPort() ) s << ":" << c.getPort() ;
            }

            trunk = s.str() ;
            chargeNumber = m_chargeNumber ;
        }
        unsigned int getCountOfTrunks() {
            return m_vecTrunks.size() ;
        }
    private:
        TerminationRoute_t() {};
        
        vector<CarrierAddressSpace_t> m_vecTrunks ;
        unsigned int m_currentTrunk ;
        string m_chargeNumber ;
    } ;
    typedef std::map<string, boost::shared_ptr<TerminationRoute_t> > TerminationCarrierMap_t ;
 
    class Appserver_t {
    public:
        Appserver_t( const string& address, unsigned int eventSocketPort = 8021 ) : m_address( address ), m_eventSocketPort(eventSocketPort) {
        }
        
        bool operator==(const Appserver_t& other) const {
            return m_address == other.m_address && m_eventSocketPort == other.m_eventSocketPort ;
        }
        
        const string& getAddress() const { return m_address ; }
        unsigned int getEventSocketPort() const { return m_eventSocketPort; }
        
        
    private:
        Appserver_t() ;
        
        string m_name ;
        string m_address ;
        unsigned int m_eventSocketPort ;
    } ;
    /* needed to be able to live in a boost unordered container */
    size_t hash_value( const Appserver_t& a) {
        std::size_t seed = 0;
        boost::hash_combine(seed, a.getAddress());
        boost::hash_combine(seed, a.getEventSocketPort());
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
    
    
    /**
     Internal implementation class
    */
    class SspConfig::Impl {
    public:
        Impl( const char* szFilename) : m_bIsValid(false), m_agent_mode(agent_mode_stateless), m_nCurrentTerminationCarrier(0), m_bIsActive(true), m_statsPort(0) {
            try {
                std::filebuf fb;
                if( !fb.open (szFilename,std::ios::in) ) {
                    cerr << "Unable to open configuration file: " << szFilename << endl ;
                    return ;
                }
                std::istream is(&fb);
                
                ptree pt;
                read_xml(is, pt);
                
                /* stats configuration */
                try {
                    pt.get_child("ssp.stats") ; // will throw if doesn't exist
                    m_statsPort = pt.get<unsigned int>("ssp.stats.<xmlattr>.port", 8022) ;
                    m_statsAddress = pt.get<string>("ssp.stats") ;
                } catch( boost::property_tree::ptree_bad_path& e ) {
                }
                
                /* logging configuration  */
                m_syslogAddress = pt.get<string>("ssp.logging.syslog.address", "localhost") ;
                m_sysLogPort = pt.get<unsigned int>("ssp.logging.syslog.address", 516) ;
                m_syslogFacility = pt.get<string>("ssp.logging.syslog.facility","local7") ;
                m_nSofiaLogLevel = pt.get<unsigned int>("ssp.logging.sofia-loglevel", 1) ;
                
                string loglevel = pt.get<string>("ssp.logging.loglevel", "info") ;
                
                if( 0 == loglevel.compare("notice") ) m_loglevel = log_notice ;
                else if( 0 == loglevel.compare("error") ) m_loglevel = log_error ;
                else if( 0 == loglevel.compare("warning") ) m_loglevel = log_warning ;
                else if( 0 == loglevel.compare("info") ) m_loglevel = log_info ;
                else if( 0 == loglevel.compare("debug") ) m_loglevel = log_debug ;
                else m_loglevel = log_info ;
                
                /* cdr database configuration */
                m_cdrUser = pt.get<string>("ssp.cdr.user","");
                m_cdrPassword = pt.get<string>("ssp.cdr.password","");
                m_cdrHost = pt.get<string>("ssp.cdr.host","");
                m_cdrPort = pt.get<string>("ssp.cdr.port","3306");
                m_cdrSchema = pt.get<string>("ssp.cdr.schema","3306");
 
                /* sip configuration */
                m_sipUrl = pt.get<string>("ssp.sip.contact", "sip:*") ;
                
                string inboundMode = pt.get<string>("ssp.sip.agent-mode", "stateful") ;
                if( 0 == inboundMode.compare("stateful") ) {
                    m_agent_mode = agent_mode_stateful ;
                }
                
                if( 0 == pt.get<string>("ss.sip.<xmlattr>.status", "active").compare("inactive") ) {
                    m_bIsActive = false ;
                }
                m_nOriginationSessionTimer = pt.get<unsigned long>("ssp.inbound.session-timer", 0) ;
                
                /* routing strategy: round robin for a configurable interval (0=always), then send the next call to least loaded server */
                m_nMaxRoundRobins = pt.get<unsigned int>("ssp.inbound.max-round-robins", 0) ;
                m_nMaxTerminationAttempts = pt.get<unsigned int>("ssp.outbound.max-termination-attempts", 1) ;
                m_nFSTimerMsecs = pt.get<unsigned long>("ssp.inbound.freeswitch-health-check-interval", 5000) ;
                
                
                string ibStrategy = pt.get<string>("ssp.routing.inbound.<xmlattr>.strategy", "") ;
                string ibTarget = pt.get<string>("ssp.routing.inbound.<xmlattr>.target", "") ;
                string obStrategy = pt.get<string>("ssp.routing.outbound.<xmlattr>.strategy", "") ;
                string obTarget = pt.get<string>("ssp.routing.outbound.<xmlattr>.route", "") ;
                                
                BOOST_FOREACH( ptree::value_type const& v, pt.get_child("ssp.inbound") ) {
                    if( v.first == "appserver" ) {
                        /* get name */
                        string appserverName ;
                        string address = v.second.data()  ;
                        if( !getXmlAttribute( v, "name", appserverName ) ) {
                            appserverName = address ;
                        }
                        
                        Appserver_t as( v.second.data(), v.second.get("event-socket-port", 8021) ) ;
                        
                        pair<AppserverMap_t::iterator, bool> ret = m_mapAppserver.insert( AppserverMap_t::value_type( appserverName, as ) ) ;
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
                                AppserverMap_t::iterator it = m_mapAppserver.find( asName )  ;
                                if( m_mapAppserver.end() == it ) {
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
                }
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

                        try {
                            BOOST_FOREACH( ptree::value_type const& v2, v.second.get_child("inbound") ) {
                                if( v2.first == "address") {
                                    CarrierAddressSpace_t c( carrierName, v2.second.data(), v2.second.get("<xmlattr>.port", 5060), v2.second.get("<xmlattr>.netmask", "255.255.255.255") ) ;
                                    pair<CarrierServerMap_t::iterator, bool> ret = m_mapInboundCarrier.insert( CarrierServerMap_t::value_type( c.getAddress(), c ) ) ;
                                    if( !ret.second ) {
                                        cerr << "Multiple carrier elements have duplicate address attribute values" << endl ;
                                        return ;
                                    }
                                    
                                }
                            }
                            
                        } catch( boost::property_tree::ptree_bad_path &e) {
                        }
                        
                        /* outbound */
                        string status = v.second.get<string>("outbound.<xmlattr>.status", "active");
                        if( 0 == status.compare("active") ) {
                            unsigned int qty =  v.second.get<unsigned int>("outbound.<xmlattr>.qty", 1) ;
                            string chargeNumber = v.second.get<string>("outbound.<xmlattr>.charge-number", "");
                            vector<CarrierAddressSpace_t> vecTrunks ;
                            try {
                                BOOST_FOREACH( ptree::value_type const& v2, v.second.get_child("outbound") ) {
                                    if( v2.first == "address") {
                                        if( 0 == v2.second.get<string>("<xmlattr>.status","active").compare("inactive") ) continue ;
                                        CarrierAddressSpace_t c( carrierName, v2.second.data(), v2.second.get("<xmlattr>.port", 5060), "255.255.255.255", 
                                            v2.second.get<unsigned int>("<xmlattr>.qty",1) )  ;
                                        vecTrunks.push_back( c ) ;
                                    }
                                }
                                if( !vecTrunks.empty() ) {
                                    boost::shared_ptr<TerminationRoute_t> t = boost::make_shared<TerminationRoute_t>( vecTrunks, chargeNumber) ;
                                    m_mapTerminationCarrierByName.insert ( TerminationCarrierMap_t::value_type( carrierName,  t) ) ;
                                    while( qty-- ) m_vecTerminationCarrier.push_back( carrierName ) ;
                                }
                                
                            } catch( boost::property_tree::ptree_bad_path &e) {
                            }
                        }
                    }
                }
                fb.close() ;
                if( 0 == m_vecTerminationCarrier.size() ) {
                    cerr << "Must have at least one termination carrier specified in the configuration file " << endl ;
                    return ;
                }
                if( 0 == m_mapAppserver.size() ) {
                    cerr << "Must have at least one appserver defined in the configuration file" << endl ;
                    return ;
                }
                                
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
        
        const string& getSipUrl() const { return m_sipUrl; }
        
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
            CarrierServerMap_t::const_iterator it = m_mapInboundCarrier.find( sourceAddress ) ;
            if( m_mapInboundCarrier.end() == it ) return false ;
            CarrierAddressSpace_t c = it->second ;
            carrier = c.getCarrier() ;
            return true ;
        }
        
        bool getSyslogFacility( sinks::syslog::facility& facility ) const {
        
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
        
        void getAppservers( deque<string>& servers) {
            for( AppserverMap_t::iterator it = m_mapAppserver.begin(); it != m_mapAppserver.end(); ++it ) {
                Appserver_t& as = it->second ;
                string s = as.getAddress() ;
                s += ":" ;
                ostringstream convert ;
                convert << as.getEventSocketPort() ;
                s += convert.str() ;
                servers.push_back( s ) ;
            }
        }
                
        bool getAppserverGroup( const string& name, AppserverGroup_t& group ) {
            boost::unordered_map< string, AppserverGroup_t>::const_iterator it = m_mapAppserverGroup.find( name ) ;
            if( m_mapAppserverGroup.end() == it ) return false ;
            
            group = it->second ;
            return true;             
        }
        unsigned int getSofiaLogLevel(void) { return m_nSofiaLogLevel; }
        unsigned int getMaxRoundRobins(void) { return m_nMaxRoundRobins; }
        
        agent_mode getAgentMode(void) { return m_agent_mode; }
        
        bool getTerminationRoute( std::string& destAddress, std::string& carrier, std::string& chargeNumber ) {
            if( m_vecTerminationCarrier.empty() ) return false ;
            carrier = m_vecTerminationCarrier.at( m_nCurrentTerminationCarrier ) ;
            TerminationCarrierMap_t::iterator it = m_mapTerminationCarrierByName.find( carrier ) ;
            if( m_mapTerminationCarrierByName.end() == it ) {
                assert(false) ;
                return false ;
            }
            boost::shared_ptr<TerminationRoute_t>& route = it->second ;
            route->getNextTrunk( destAddress, chargeNumber ) ;
            if( ++m_nCurrentTerminationCarrier >= m_vecTerminationCarrier.size() ) m_nCurrentTerminationCarrier = 0 ;
            return true;
        }

        /* try to find a different carrier for a failed attempt */
        bool getTerminationRouteForAltCarrier( const std::string& failedCarrier, std::string& destAddress, std::string& carrier, std::string& chargeNumber ) {
            for( TerminationCarrierMap_t::iterator it = m_mapTerminationCarrierByName.begin(); it != m_mapTerminationCarrierByName.end(); it++ ) {
                carrier = it->first ;
                if( 0 != carrier.compare(failedCarrier) ) {
                    boost::shared_ptr<TerminationRoute_t>& route = it->second ;
                    route->getNextTrunk( destAddress, chargeNumber ) ;   
                    return true ;             
                }
            }

            /* may not have multiple carriers, just return the next route */
            return getTerminationRoute( destAddress, carrier, chargeNumber ) ;
        }
        
        bool isActive() {
            return m_bIsActive ;
        }
        unsigned int getCountOfOutboundTrunks() {
            int count = 0 ;
            for( TerminationCarrierMap_t::iterator it = m_mapTerminationCarrierByName.begin(); it != m_mapTerminationCarrierByName.end(); it++ ) {
                boost::shared_ptr<TerminationRoute_t>& route = it->second ;
                count += route->getCountOfTrunks();
            }
            return count ;
        }
        unsigned int getMaxTerminationAttempts() {
            return m_nMaxTerminationAttempts ;
        }
        unsigned long getOriginationSessionTimer() {
            return m_nOriginationSessionTimer ;
        }
        unsigned long getFSHealthCheckTimerTimeMsecs() {
            return m_nFSTimerMsecs ;
        }
        severity_levels getLoglevel() {
            return m_loglevel ;
        }
        unsigned int getStatsPort( string& address ) {
            address = m_statsAddress ;
            return m_statsPort ;
        }
        bool getCdrConnectInfo( string& user, string& pass, string& dbUrl, string& schema ) {
            if( 0 == m_cdrHost.length() || 0 == m_cdrUser.length() || 0 == m_cdrPassword.length() || 0 == schema.length() ) return false ;

            user = m_cdrUser ;
            pass = m_cdrPassword ;
            schema = m_cdrSchema ;
            ostringstream s ;
            s << "tcp://" << m_cdrHost << ":" << m_cdrPort ;
            dbUrl = s.str() ;
            return true; 
        }

    private:
        
        bool getXmlAttribute( ptree::value_type const& v, const string& attrName, string& value ) {
            try {
                string key = "<xmlattr>." ;
                key.append( attrName ) ;
                value = v.second.get<string>( key ) ;
            } catch( const ptree_error& err ) {
                return false ;
            }
            if( value.empty() ) return false ;
            return true ;
        }
    
        
        
        bool m_bIsValid ;
        bool m_bIsActive ;
        string m_syslogAddress ;
        unsigned int m_sysLogPort ;
        string m_syslogFacility ;
        string m_sipUrl ;
        CustomerDnisMap_t m_mapCustomerDnis ;
        CarrierServerMap_t m_mapInboundCarrier ;
        CarrierServerMap_t m_mapOutboundCarrier ;
        TerminationCarrierMap_t m_mapTerminationCarrierByName ;
        vector<string> m_vecTerminationCarrier ;
        unsigned int m_nCurrentTerminationCarrier ;
        AppserverMap_t m_mapAppserver;
        AppserverGroupMap_t m_mapAppserverGroup ;
        agent_mode m_agent_mode ;
        unsigned int m_nSofiaLogLevel ;
        unsigned int m_nMaxRoundRobins ;
        unsigned int m_nMaxTerminationAttempts;
        unsigned long m_nOriginationSessionTimer ;
        unsigned long m_nFSTimerMsecs ;
        severity_levels m_loglevel ;
        string m_statsAddress ;
        unsigned int m_statsPort ;
        string m_cdrUser ;
        string m_cdrPassword ;
        string m_cdrHost ;
        string m_cdrPort ;
        string m_cdrSchema ;
    } ;
    
    /*
     Public interface
    */
    SspConfig::SspConfig( const char* szFilename ) : m_pimpl( new Impl(szFilename) ) {
    }
    
    SspConfig::~SspConfig() {
       delete m_pimpl ;
    }
    
    bool SspConfig::isValid() {
        return m_pimpl->isValid() ;
    }
    
    bool SspConfig::getSipUrl( std::string& sipUrl ) const {
        sipUrl = m_pimpl->getSipUrl() ;
        return true ;
    }
    
    bool SspConfig::getSyslogTarget( std::string& address, unsigned int& port ) const {
        return m_pimpl->getSyslogTarget( address, port ) ;
    }
    bool SspConfig::getCustomer( const std::string& dnis, std::string& customer) const {
        return m_pimpl->getCustomer( dnis, customer ) ;
    }
    bool SspConfig::getCarrier( const std::string& sourceAddress, std::string& customer) const {
        return m_pimpl->getCarrier( sourceAddress, customer ) ;
    }

    bool SspConfig::getSyslogFacility( sinks::syslog::facility& facility ) const {
        return m_pimpl->getSyslogFacility( facility ) ;
    }
        
    void SspConfig::getAppservers( deque<string>& servers) {
        return m_pimpl->getAppservers(servers) ;
    }
    
    agent_mode SspConfig::getAgentMode() {
        return m_pimpl->getAgentMode() ;
    }
    unsigned int SspConfig::getSofiaLogLevel() {
        return m_pimpl->getSofiaLogLevel() ;
    }
    unsigned int SspConfig::getMaxRoundRobins() {
        return m_pimpl->getMaxRoundRobins() ;
    }
    bool SspConfig::getTerminationRoute( std::string& destAddress, std::string& carrier, std::string& chargeNumber ) {
        return m_pimpl->getTerminationRoute( destAddress, carrier, chargeNumber ) ;
    }
    bool SspConfig::getTerminationRouteForAltCarrier( const std::string& failedCarrier, std::string& destAddress, std::string& carrier, std::string& chargeNumber ) {
        return m_pimpl->getTerminationRouteForAltCarrier( failedCarrier, destAddress, carrier, chargeNumber ) ;
    }
    bool SspConfig::isActive() {
        return m_pimpl->isActive() ;
    }
    unsigned int SspConfig::getCountOfOutboundTrunks(void) {
        return m_pimpl->getCountOfOutboundTrunks(); 
    }
    unsigned int SspConfig::getMaxTerminationAttempts(void) {
        return m_pimpl->getMaxTerminationAttempts() ;
    }
    unsigned long SspConfig::getOriginationSessionTimer() {
        return m_pimpl->getOriginationSessionTimer() ;
    }
    unsigned long SspConfig::getFSHealthCheckTimerTimeMsecs(void) {
        return m_pimpl->getFSHealthCheckTimerTimeMsecs() ;
    }
    bool SspConfig::getCdrConnectInfo( string& user, string& pass, string& dbUrl, string& schema ) {
        return m_pimpl->getCdrConnectInfo( user, pass, dbUrl, schema ) ;
    } 
    void SspConfig::Log() const {
        SSP_LOG(log_notice) << "Configuration:" << endl ;
    }
    severity_levels SspConfig::getLoglevel() {
        return m_pimpl->getLoglevel() ;
    }
    unsigned int SspConfig::getStatsPort( string& address ) {
        return m_pimpl->getStatsPort( address ) ;
    }

}
