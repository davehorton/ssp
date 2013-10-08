#define GIT_VERSION(x) #x

char const * version() 
{ 
     return GIT_VERSION(VERSION); 
} 