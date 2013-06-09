//
//  fs-message.h
//  ssp
//
//  Created by Dave Horton on 6/8/13.
//  Copyright (c) 2013 Beachdog Networks. All rights reserved.
//

#ifndef __ssp__fs_message__
#define __ssp__fs_message__

#include <iostream>

#include "ssp.h"

using namespace std ;

namespace ssp {
    class FsMessage {
    public:
        FsMessage() ;
        ~FsMessage() ;
 
        enum FS_MSG_CATEGORY {
            unknown_category,
            api,
            auth,
            command,
            text
        } ;
        
        enum FS_MSG_TYPE {
            unknown_type,
            request,
            reply,
            response,
            disconnect_notice
        } ;
        
        enum FS_REPLY_STATUS {
            undefined,
            ok,
            err
        } ;

        bool append( const string& data ) ;
        bool isValid() const { return m_bValid; }
        bool isComplete() const { return m_bComplete; }
        bool isEmpty() const { 0 == m_rawMsg.length() ; }
    
        FS_MSG_CATEGORY getCategory() const { return m_category ; }
        FS_MSG_TYPE getType() const { return m_type ; }
        bool getReplyText( string& strReplyText ) {
            if( 0 == m_strReplyText.length() ) return false ;
            strReplyText = m_strReplyText ;
            return true ;
        }
        bool getContent( string& strContent ) {
            if( 0 == m_nContentLength ) return false ;
            strContent = m_strContent ;
            return true ;
        }
        FS_REPLY_STATUS getReplyStatus() const { return m_replyStatus; }
        
        bool getSipProfile( const string& profile, string& address, unsigned int port ) const ;
        bool getFsStatus( unsigned int& nCurrentSessions, unsigned int& nMaxSessions ) const ;
        
        
    protected:
        void reset() ;
        void parse( const string& data ) throw(string) ;
        
    private:
        
        bool parseLeadingInteger( const string& s, unsigned int& number) const ;
        
        FS_MSG_CATEGORY m_category ;
        FS_MSG_TYPE     m_type ;
        FS_REPLY_STATUS m_replyStatus ;
        string          m_strReplyText ;
        unsigned int    m_nContentLength ;
        string          m_strContent ;
        string          m_rawMsg ;
        bool            m_bValid ;
        bool            m_bComplete ;
    } ;
}
#endif /* defined(__ssp__fs_message__) */
