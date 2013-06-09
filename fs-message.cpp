//
//  fs-message.cpp
//  ssp
//
//  Created by Dave Horton on 6/8/13.
//  Copyright (c) 2013 Beachdog Networks. All rights reserved.
//
#include <boost/tokenizer.hpp>

#include "fs-message.h"
#include "ssp-controller.h"

namespace ssp {
    FsMessage::FsMessage() : m_bValid(false), m_bComplete(false) {
        reset() ;
    }
    FsMessage::~FsMessage() {
        
    }
    
    bool FsMessage::append( const string& data ) {
        m_rawMsg.append( data ) ;
        try {
            parse( data ) ;
            m_bValid = true ;
        } catch( string& e ) {
            SSP_LOG(log_error) << "Error parsing freeswitch msg: " << data << ": " << e << endl ;
        }
        return m_bComplete ;
    }
    
    void FsMessage::reset() {
        m_category = unknown_category ;
        m_type = unknown_type ;
        m_replyStatus = undefined ;
        m_rawMsg.clear() ;
        m_strContent.clear() ;
        m_strReplyText.clear() ;
        m_nContentLength = 0 ;
        m_bComplete = false ;
        m_bValid = false ;
    }
    
    void FsMessage::parse( const string& data ) throw(string) {

        if( m_nContentLength > 0 ) {
            m_strContent.append( data ) ;
        }
        else {
            boost::char_separator<char> sep("\n") ;
            tokenizer tok( m_rawMsg, sep) ;
            int i = 0 ;
            for( tokenizer::iterator it = tok.begin(); it != tok.end(); ++i, ++it ) {
                const string& line = *it ;
                if( unknown_category == m_category ) {
                    if( 0 != line.find("Content-Type: ")) throw("Invalid msg: Expected Content-Type on first line: " + line) ;
                    
                    string s = line.substr(14) ;
                    tokenizer tok2(s, boost::char_separator<char>("/") ) ;
                    if( 2 != distance(tok2.begin(), tok2.end()) ) throw("Invalid Content-Type: " + s) ;
                    
                    tokenizer::iterator it2 = tok2.begin() ;
                    if( 0 == (*it2).compare("api")) m_category = api ;
                    else if( 0 == (*it2).compare("auth")) m_category = auth ;
                    else if( 0 == (*it2).compare("command")) m_category = command ;
                    else if( 0 == (*it2).compare("text")) m_category = text ;
                    else throw("Invalid/unknown Content-Type: '" + line + "': " + (*it2) + " is an unknown category") ;
                    
                    ++it2 ;
                    if( 0 == (*it2).compare("reply") )  m_type = reply ;
                    else if( 0 == (*it2).compare("request") )  m_type = request ;
                    else if( 0 == (*it2).compare("response") )  m_type = response ;
                    else if( 0 == (*it2).compare("disconnect-notice") )  m_type = disconnect_notice ;
                    else throw("Invalid/unknown Content-Type: '" + line + "': " + (*it2) + " is an unknown type") ;
                }
                else if( command == m_category && reply == m_type ) {
                    if( undefined != m_replyStatus ) throw("Invalid msg: multiple lines of Reply-Text: " + line) ;
                    if( 0 != line.find("Reply-Text: ")) throw("Invalid msg: Expected Reply-Text on second line of command/reply msg: " + line) ;
                    m_strReplyText = line ;
                    string s = line.substr(12) ;
                    if( 0 == s.find("+OK") ) m_replyStatus = ok ;
                    else if( 0 == s.find("+ERR")) m_replyStatus = err ;
                    else throw("Invalid reply status: " + s ) ;
                }
                else if( (api == m_category && response == m_type) || text == m_category ) {
                    if( 0 == m_nContentLength ) {
                        if( 0 != line.find("Content-Length: ")) throw("Invalid msg: Expected Content-Length on second line of api/response or text msg: " + line) ;
                        string s = line.substr(16) ;
                        m_nContentLength = atoi( s.c_str() ) ;
                    }
                    else {
                        m_strContent.append( line ) ;
                        m_strContent.append( "\n" ) ;
                    }
                }
            }
           
        }
  
        if( request == m_type ||
           reply == m_type && undefined != m_replyStatus ||
           (m_nContentLength> 0 && m_strContent.length() == m_nContentLength ) ) {
            
            m_bComplete = true ;
        }
        else {
            SSP_LOG(log_debug) << "read partial message; Content-Length is " << m_nContentLength << " but read " << m_strContent.length() << endl ;
        }
    }
    
    bool FsMessage::getSipProfile( const string& profile, string& address, unsigned int& port ) const {
        //SSP_LOG(log_debug) << "searching for profile " << profile << " in content " << m_strContent << endl ;
        tokenizer tok( m_strContent, boost::char_separator<char>("\n")) ;
        for( tokenizer::iterator it = tok.begin(); it != tok.end(); ++it ) {
            //SSP_LOG(log_debug) << "line: " << *it << endl ;
            tokenizer tok2( *it, boost::char_separator<char>(" \t")) ;
            tokenizer::iterator it2 = tok2.begin() ;
            //SSP_LOG(log_debug) << "first token: " << *it2 << endl ;
            if( 0 == (*it2).compare( profile ) ) {
                advance( it2, 2 ) ;
                const string& contact = *it2 ;
                //SSP_LOG(log_debug) << "contact: " << contact << endl ;
                tokenizer tok3( *it2,  boost::char_separator<char>("@:")) ;
                tokenizer::iterator it3 = tok3.begin() ;
                advance(it3, 2) ;
                address = *it3 ;
                if( ++it3 != tok3.end() ) port = atoi( (*it3).c_str() ) ;
                else port = 5060 ;
                return true ;
            }
        }
        
        return false ;
    }
    bool FsMessage::getFsStatus( unsigned int& nCurrentSessions, unsigned int& nMaxSessions ) const {
        tokenizer tok( m_strContent, boost::char_separator<char>("\n")) ;
        tokenizer::iterator it = tok.begin();
        
        if( distance( tok.begin(), tok.end() ) < 5 ) {
            SSP_LOG(log_debug) << "expected at least 5 lines of status, instead got " << distance( tok.begin(), tok.end() ) << endl ;
            return false ;
        } ;
        advance( it, 3 ) ;
        if( !parseLeadingInteger(*it, nCurrentSessions) ) {
            return false ;
        }
        advance( it, 1 ) ;
        if( !parseLeadingInteger(*it, nMaxSessions) ) {
            return false ;
        }
        
        return true ;
    }
    bool FsMessage::parseLeadingInteger( const string& str, unsigned int& number) const {
        //SSP_LOG(log_debug) << "parsing integer from '" << str << "'" << endl ;
        tokenizer tok( str, boost::char_separator<char>(" ") ) ;
        tokenizer::iterator it = tok.begin() ;
        string s = *it;
        //SSP_LOG(log_debug) << "proposed integer value is '" << s << "'" << endl ;
        if( !s.empty() && std::find_if(s.begin(), s.end(), ::isdigit ) != s.end() ) {
            number = ::atoi( s.c_str() ) ;
            return true ;
        }
        //SSP_LOG(log_debug) << "invalid integer string " << str << endl ;
        return false ;    }

}
