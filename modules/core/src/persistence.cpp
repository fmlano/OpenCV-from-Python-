/*M///////////////////////////////////////////////////////////////////////////////////////
//
//  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
//
//  By downloading, copying, installing or using the software you agree to this license.
//  If you do not agree to this license, do not download, install,
//  copy or use the software.
//
//
//                           License Agreement
//                For Open Source Computer Vision Library
//
// Copyright (C) 2000-2008, Intel Corporation, all rights reserved.
// Copyright (C) 2009, Willow Garage Inc., all rights reserved.
// Third party copyrights are property of their respective owners.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//   * Redistribution's of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//   * Redistribution's in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
//   * The name of the copyright holders may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// This software is provided by the copyright holders and contributors "as is" and
// any express or implied warranties, including, but not limited to, the implied
// warranties of merchantability and fitness for a particular purpose are disclaimed.
// In no event shall the Intel Corporation or contributors be liable for any direct,
// indirect, incidental, special, exemplary, or consequential damages
// (including, but not limited to, procurement of substitute goods or services;
// loss of use, data, or profits; or business interruption) however caused
// and on any theory of liability, whether in contract, strict liability,
// or tort (including negligence or otherwise) arising in any way out of
// the use of this software, even if advised of the possibility of such damage.
//
//M*/

#include "precomp.hpp"
#include "persistence.hpp"
#include <ctype.h>
#include <deque>
#include <sstream>
#include <string>
#include <iterator>

char* icv_itoa( int _val, char* buffer, int /*radix*/ )
{
    const int radix = 10;
    char* ptr=buffer + 23 /* enough even for 64-bit integers */;
    unsigned val = abs(_val);

    *ptr = '\0';
    do
    {
        unsigned r = val / radix;
        *--ptr = (char)(val - (r*radix) + '0');
        val = r;
    }
    while( val != 0 );

    if( _val < 0 )
        *--ptr = '-';

    return ptr;
}

static inline bool cv_strcasecmp(const char * s1, const char * s2)
{
    if ( s1 == 0 && s2 == 0 )
        return true;
    else if ( s1 == 0 || s2 == 0 )
        return false;

    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);
    if ( len1 != len2 )
        return false;

    for ( size_t i = 0U; i < len1; i++ )
        if ( tolower( static_cast<int>(s1[i]) ) != tolower( static_cast<int>(s2[i]) ) )
            return false;

    return true;
}

cv::String cv::FileStorage::getDefaultObjectName(const cv::String& _filename)
{
    static const char* stubname = "unnamed";
    const char* filename = _filename.c_str();
    const char* ptr2 = filename + _filename.size();
    const char* ptr = ptr2 - 1;
    cv::AutoBuffer<char> name_buf(_filename.size()+1);

    while( ptr >= filename && *ptr != '\\' && *ptr != '/' && *ptr != ':' )
    {
        if( *ptr == '.' && (!*ptr2 || strncmp(ptr2, ".gz", 3) == 0) )
            ptr2 = ptr;
        ptr--;
    }
    ptr++;
    if( ptr == ptr2 )
        CV_Error( CV_StsBadArg, "Invalid filename" );

    char* name = name_buf;

    // name must start with letter or '_'
    if( !cv_isalpha(*ptr) && *ptr!= '_' ){
        *name++ = '_';
    }

    while( ptr < ptr2 )
    {
        char c = *ptr++;
        if( !cv_isalnum(c) && c != '-' && c != '_' )
            c = '_';
        *name++ = c;
    }
    *name = '\0';
    name = name_buf;
    if( strcmp( name, "_" ) == 0 )
        strcpy( name, stubname );
    return String(name);
}

typedef struct CvFileMapNode
{
    CvFileNode value;
    const CvStringHashNode* key;
    struct CvFileMapNode* next;
}
CvFileMapNode;





void icvPuts( CvFileStorage* fs, const char* str )
{
    if( fs->outbuf )
        std::copy(str, str + strlen(str), std::back_inserter(*fs->outbuf));
    else if( fs->file )
        fputs( str, fs->file );
#if USE_ZLIB
    else if( fs->gzfile )
        gzputs( fs->gzfile, str );
#endif
    else
        CV_Error( CV_StsError, "The storage is not opened" );
}

char* icvGets( CvFileStorage* fs, char* str, int maxCount )
{
    if( fs->strbuf )
    {
        size_t i = fs->strbufpos, len = fs->strbufsize;
        int j = 0;
        const char* instr = fs->strbuf;
        while( i < len && j < maxCount-1 )
        {
            char c = instr[i++];
            if( c == '\0' )
                break;
            str[j++] = c;
            if( c == '\n' )
                break;
        }
        str[j++] = '\0';
        fs->strbufpos = i;
        return j > 1 ? str : 0;
    }
    if( fs->file )
        return fgets( str, maxCount, fs->file );
#if USE_ZLIB
    if( fs->gzfile )
        return gzgets( fs->gzfile, str, maxCount );
#endif
    CV_Error( CV_StsError, "The storage is not opened" );
    return 0;
}

int icvEof( CvFileStorage* fs )
{
    if( fs->strbuf )
        return fs->strbufpos >= fs->strbufsize;
    if( fs->file )
        return feof(fs->file);
#if USE_ZLIB
    if( fs->gzfile )
        return gzeof(fs->gzfile);
#endif
    return false;
}

void icvCloseFile( CvFileStorage* fs )
{
    if( fs->file )
        fclose( fs->file );
#if USE_ZLIB
    else if( fs->gzfile )
        gzclose( fs->gzfile );
#endif
    fs->file = 0;
    fs->gzfile = 0;
    fs->strbuf = 0;
    fs->strbufpos = 0;
    fs->is_opened = false;
}

void icvRewind( CvFileStorage* fs )
{
    if( fs->file )
        rewind(fs->file);
#if USE_ZLIB
    else if( fs->gzfile )
        gzrewind(fs->gzfile);
#endif
    fs->strbufpos = 0;
}


CV_IMPL const char*
cvAttrValue( const CvAttrList* attr, const char* attr_name )
{
    while( attr && attr->attr )
    {
        int i;
        for( i = 0; attr->attr[i*2] != 0; i++ )
        {
            if( strcmp( attr_name, attr->attr[i*2] ) == 0 )
                return attr->attr[i*2+1];
        }
        attr = attr->next;
    }

    return 0;
}


static CvGenericHash*
cvCreateMap( int flags, int header_size, int elem_size,
             CvMemStorage* storage, int start_tab_size )
{
    if( header_size < (int)sizeof(CvGenericHash) )
        CV_Error( CV_StsBadSize, "Too small map header_size" );

    if( start_tab_size <= 0 )
        start_tab_size = 16;

    CvGenericHash* map = (CvGenericHash*)cvCreateSet( flags, header_size, elem_size, storage );

    map->tab_size = start_tab_size;
    start_tab_size *= sizeof(map->table[0]);
    map->table = (void**)cvMemStorageAlloc( storage, start_tab_size );
    memset( map->table, 0, start_tab_size );

    return map;
}

void icvParseError( CvFileStorage* fs, const char* func_name,
               const char* err_msg, const char* source_file, int source_line )
{
    char buf[1<<10];
    sprintf( buf, "%s(%d): %s", fs->filename, fs->lineno, err_msg );
    cvError( CV_StsParseError, func_name, buf, source_file, source_line );
}


void icvFSCreateCollection( CvFileStorage* fs, int tag, CvFileNode* collection )
{
    if( CV_NODE_IS_MAP(tag) )
    {
        if( collection->tag != CV_NODE_NONE )
        {
            assert( fs->fmt == CV_STORAGE_FORMAT_XML );
            CV_PARSE_ERROR( "Sequence element should not have name (use <_></_>)" );
        }

        collection->data.map = cvCreateMap( 0, sizeof(CvFileNodeHash),
                            sizeof(CvFileMapNode), fs->memstorage, 16 );
    }
    else
    {
        CvSeq* seq;
        seq = cvCreateSeq( 0, sizeof(CvSeq), sizeof(CvFileNode), fs->memstorage );

        // if <collection> contains some scalar element, add it to the newly created collection
        if( CV_NODE_TYPE(collection->tag) != CV_NODE_NONE )
            cvSeqPush( seq, collection );

        collection->data.seq = seq;
    }

    collection->tag = tag;
    cvSetSeqBlockSize( collection->data.seq, 8 );
}


/*static void
icvFSReleaseCollection( CvSeq* seq )
{
    if( seq )
    {
        int is_map = CV_IS_SET(seq);
        CvSeqReader reader;
        int i, total = seq->total;
        cvStartReadSeq( seq, &reader, 0 );

        for( i = 0; i < total; i++ )
        {
            CvFileNode* node = (CvFileNode*)reader.ptr;

            if( (!is_map || CV_IS_SET_ELEM( node )) && CV_NODE_IS_COLLECTION(node->tag) )
            {
                if( CV_NODE_IS_USER(node->tag) && node->info && node->data.obj.decoded )
                    cvRelease( (void**)&node->data.obj.decoded );
                if( !CV_NODE_SEQ_IS_SIMPLE( node->data.seq ))
                    icvFSReleaseCollection( node->data.seq );
            }
            CV_NEXT_SEQ_ELEM( seq->elem_size, reader );
        }
    }
}*/


static char*
icvFSDoResize( CvFileStorage* fs, char* ptr, int len )
{
    char* new_ptr = 0;
    int written_len = (int)(ptr - fs->buffer_start);
    int new_size = (int)((fs->buffer_end - fs->buffer_start)*3/2);
    new_size = MAX( written_len + len, new_size );
    new_ptr = (char*)cvAlloc( new_size + 256 );
    fs->buffer = new_ptr + (fs->buffer - fs->buffer_start);
    if( written_len > 0 )
        memcpy( new_ptr, fs->buffer_start, written_len );
    fs->buffer_start = new_ptr;
    fs->buffer_end = fs->buffer_start + new_size;
    new_ptr += written_len;
    return new_ptr;
}


char* icvFSResizeWriteBuffer( CvFileStorage* fs, char* ptr, int len )
{
    return ptr + len < fs->buffer_end ? ptr : icvFSDoResize( fs, ptr, len );
}


char* icvFSFlush( CvFileStorage* fs )
{
    char* ptr = fs->buffer;
    int indent;

    if( ptr > fs->buffer_start + fs->space )
    {
        ptr[0] = '\n';
        ptr[1] = '\0';
        icvPuts( fs, fs->buffer_start );
        fs->buffer = fs->buffer_start;
    }

    indent = fs->struct_indent;

    if( fs->space != indent )
    {
        memset( fs->buffer_start, ' ', indent );
        fs->space = indent;
    }

    ptr = fs->buffer = fs->buffer_start + fs->space;

    return ptr;
}


static void
icvClose( CvFileStorage* fs, cv::String* out )
{
    if( out )
        out->clear();

    if( !fs )
        CV_Error( CV_StsNullPtr, "NULL double pointer to file storage" );

    if( fs->is_opened )
    {
        if( fs->write_mode && (fs->file || fs->gzfile || fs->outbuf) )
        {
            if( fs->write_stack )
            {
                while( fs->write_stack->total > 0 )
                    cvEndWriteStruct(fs);
            }
            icvFSFlush(fs);
            if( fs->fmt == CV_STORAGE_FORMAT_XML )
                icvPuts( fs, "</opencv_storage>\n" );
            else if ( fs->fmt == CV_STORAGE_FORMAT_JSON )
                icvPuts( fs, "}\n" );
        }

        icvCloseFile(fs);
    }

    if( fs->outbuf && out )
    {
        *out = cv::String(fs->outbuf->begin(), fs->outbuf->end());
    }
}


/* closes file storage and deallocates buffers */
CV_IMPL  void
cvReleaseFileStorage( CvFileStorage** p_fs )
{
    if( !p_fs )
        CV_Error( CV_StsNullPtr, "NULL double pointer to file storage" );

    if( *p_fs )
    {
        CvFileStorage* fs = *p_fs;
        *p_fs = 0;

        icvClose(fs, 0);

        cvReleaseMemStorage( &fs->strstorage );
        cvFree( &fs->buffer_start );
        cvReleaseMemStorage( &fs->memstorage );

        delete fs->outbuf;
        delete fs->base64_writer;
        delete[] fs->delayed_struct_key;
        delete[] fs->delayed_type_name;

        memset( fs, 0, sizeof(*fs) );
        cvFree( &fs );
    }
}


#define CV_HASHVAL_SCALE 33

CV_IMPL CvStringHashNode*
cvGetHashedKey( CvFileStorage* fs, const char* str, int len, int create_missing )
{
    CvStringHashNode* node = 0;
    unsigned hashval = 0;
    int i, tab_size;

    if( !fs )
        return 0;

    CvStringHash* map = fs->str_hash;

    if( len < 0 )
    {
        for( i = 0; str[i] != '\0'; i++ )
            hashval = hashval*CV_HASHVAL_SCALE + (unsigned char)str[i];
        len = i;
    }
    else for( i = 0; i < len; i++ )
        hashval = hashval*CV_HASHVAL_SCALE + (unsigned char)str[i];

    hashval &= INT_MAX;
    tab_size = map->tab_size;
    if( (tab_size & (tab_size - 1)) == 0 )
        i = (int)(hashval & (tab_size - 1));
    else
        i = (int)(hashval % tab_size);

    for( node = (CvStringHashNode*)(map->table[i]); node != 0; node = node->next )
    {
        if( node->hashval == hashval &&
            node->str.len == len &&
            memcmp( node->str.ptr, str, len ) == 0 )
            break;
    }

    if( !node && create_missing )
    {
        node = (CvStringHashNode*)cvSetNew( (CvSet*)map );
        node->hashval = hashval;
        node->str = cvMemStorageAllocString( map->storage, str, len );
        node->next = (CvStringHashNode*)(map->table[i]);
        map->table[i] = node;
    }

    return node;
}


CV_IMPL CvFileNode*
cvGetFileNode( CvFileStorage* fs, CvFileNode* _map_node,
               const CvStringHashNode* key,
               int create_missing )
{
    CvFileNode* value = 0;
    int k = 0, attempts = 1;

    if( !fs )
        return 0;

    CV_CHECK_FILE_STORAGE(fs);

    if( !key )
        CV_Error( CV_StsNullPtr, "Null key element" );

    if( _map_node )
    {
        if( !fs->roots )
            return 0;
        attempts = fs->roots->total;
    }

    for( k = 0; k < attempts; k++ )
    {
        int i, tab_size;
        CvFileNode* map_node = _map_node;
        CvFileMapNode* another;
        CvFileNodeHash* map;

        if( !map_node )
            map_node = (CvFileNode*)cvGetSeqElem( fs->roots, k );
        CV_Assert(map_node != NULL);
        if( !CV_NODE_IS_MAP(map_node->tag) )
        {
            if( (!CV_NODE_IS_SEQ(map_node->tag) || map_node->data.seq->total != 0) &&
                CV_NODE_TYPE(map_node->tag) != CV_NODE_NONE )
                CV_Error( CV_StsError, "The node is neither a map nor an empty collection" );
            return 0;
        }

        map = map_node->data.map;
        tab_size = map->tab_size;

        if( (tab_size & (tab_size - 1)) == 0 )
            i = (int)(key->hashval & (tab_size - 1));
        else
            i = (int)(key->hashval % tab_size);

        for( another = (CvFileMapNode*)(map->table[i]); another != 0; another = another->next )
            if( another->key == key )
            {
                if( !create_missing )
                {
                    value = &another->value;
                    return value;
                }
                CV_PARSE_ERROR( "Duplicated key" );
            }

        if( k == attempts - 1 && create_missing )
        {
            CvFileMapNode* node = (CvFileMapNode*)cvSetNew( (CvSet*)map );
            node->key = key;

            node->next = (CvFileMapNode*)(map->table[i]);
            map->table[i] = node;
            value = (CvFileNode*)node;
        }
    }

    return value;
}


CV_IMPL CvFileNode*
cvGetFileNodeByName( const CvFileStorage* fs, const CvFileNode* _map_node, const char* str )
{
    CvFileNode* value = 0;
    int i, len, tab_size;
    unsigned hashval = 0;
    int k = 0, attempts = 1;

    if( !fs )
        return 0;

    CV_CHECK_FILE_STORAGE(fs);

    if( !str )
        CV_Error( CV_StsNullPtr, "Null element name" );

    for( i = 0; str[i] != '\0'; i++ )
        hashval = hashval*CV_HASHVAL_SCALE + (unsigned char)str[i];
    hashval &= INT_MAX;
    len = i;

    if( !_map_node )
    {
        if( !fs->roots )
            return 0;
        attempts = fs->roots->total;
    }

    for( k = 0; k < attempts; k++ )
    {
        CvFileNodeHash* map;
        const CvFileNode* map_node = _map_node;
        CvFileMapNode* another;

        if( !map_node )
            map_node = (CvFileNode*)cvGetSeqElem( fs->roots, k );

        if( !CV_NODE_IS_MAP(map_node->tag) )
        {
            if( (!CV_NODE_IS_SEQ(map_node->tag) || map_node->data.seq->total != 0) &&
                CV_NODE_TYPE(map_node->tag) != CV_NODE_NONE )
                CV_Error( CV_StsError, "The node is neither a map nor an empty collection" );
            return 0;
        }

        map = map_node->data.map;
        tab_size = map->tab_size;

        if( (tab_size & (tab_size - 1)) == 0 )
            i = (int)(hashval & (tab_size - 1));
        else
            i = (int)(hashval % tab_size);

        for( another = (CvFileMapNode*)(map->table[i]); another != 0; another = another->next )
        {
            const CvStringHashNode* key = another->key;

            if( key->hashval == hashval &&
                key->str.len == len &&
                memcmp( key->str.ptr, str, len ) == 0 )
            {
                value = &another->value;
                return value;
            }
        }
    }

    return value;
}


CV_IMPL CvFileNode*
cvGetRootFileNode( const CvFileStorage* fs, int stream_index )
{
    CV_CHECK_FILE_STORAGE(fs);

    if( !fs->roots || (unsigned)stream_index >= (unsigned)fs->roots->total )
        return 0;

    return (CvFileNode*)cvGetSeqElem( fs->roots, stream_index );
}


/* returns the sequence element by its index */
/*CV_IMPL CvFileNode*
cvGetFileNodeFromSeq( CvFileStorage* fs,
                      CvFileNode* seq_node, int index )
{
    CvFileNode* value = 0;
    CvSeq* seq;

    if( !seq_node )
        seq = fs->roots;
    else if( !CV_NODE_IS_SEQ(seq_node->tag) )
    {
        if( CV_NODE_IS_MAP(seq_node->tag) )
            CV_Error( CV_StsError, "The node is map. Use cvGetFileNodeFromMap()." );
        if( CV_NODE_TYPE(seq_node->tag) == CV_NODE_NONE )
            CV_Error( CV_StsError, "The node is an empty object (None)." );
        if( index != 0 && index != -1 )
            CV_Error( CV_StsOutOfRange, "" );
        value = seq_node;
        EXIT;
    }
    else
        seq = seq_node->data.seq;

    if( !seq )
        CV_Error( CV_StsNullPtr, "The file storage is empty" );

    value = (CvFileNode*)cvGetSeqElem( seq, index, 0 );



    return value;
}*/


char* icvDoubleToString( char* buf, double value )
{
    Cv64suf val;
    unsigned ieee754_hi;

    val.f = value;
    ieee754_hi = (unsigned)(val.u >> 32);

    if( (ieee754_hi & 0x7ff00000) != 0x7ff00000 )
    {
        int ivalue = cvRound(value);
        if( ivalue == value )
            sprintf( buf, "%d.", ivalue );
        else
        {
            static const char* fmt = "%.16e";
            char* ptr = buf;
            sprintf( buf, fmt, value );
            if( *ptr == '+' || *ptr == '-' )
                ptr++;
            for( ; cv_isdigit(*ptr); ptr++ )
                ;
            if( *ptr == ',' )
                *ptr = '.';
        }
    }
    else
    {
        unsigned ieee754_lo = (unsigned)val.u;
        if( (ieee754_hi & 0x7fffffff) + (ieee754_lo != 0) > 0x7ff00000 )
            strcpy( buf, ".Nan" );
        else
            strcpy( buf, (int)ieee754_hi < 0 ? "-.Inf" : ".Inf" );
    }

    return buf;
}


static char*
icvFloatToString( char* buf, float value )
{
    Cv32suf val;
    unsigned ieee754;
    val.f = value;
    ieee754 = val.u;

    if( (ieee754 & 0x7f800000) != 0x7f800000 )
    {
        int ivalue = cvRound(value);
        if( ivalue == value )
            sprintf( buf, "%d.", ivalue );
        else
        {
            static const char* fmt = "%.8e";
            char* ptr = buf;
            sprintf( buf, fmt, value );
            if( *ptr == '+' || *ptr == '-' )
                ptr++;
            for( ; cv_isdigit(*ptr); ptr++ )
                ;
            if( *ptr == ',' )
                *ptr = '.';
        }
    }
    else
    {
        if( (ieee754 & 0x7fffffff) != 0x7f800000 )
            strcpy( buf, ".Nan" );
        else
            strcpy( buf, (int)ieee754 < 0 ? "-.Inf" : ".Inf" );
    }

    return buf;
}


static void
icvProcessSpecialDouble( CvFileStorage* fs, char* buf, double* value, char** endptr )
{
    char c = buf[0];
    int inf_hi = 0x7ff00000;

    if( c == '-' || c == '+' )
    {
        inf_hi = c == '-' ? 0xfff00000 : 0x7ff00000;
        c = *++buf;
    }

    if( c != '.' )
        CV_PARSE_ERROR( "Bad format of floating-point constant" );

    union{double d; uint64 i;} v;
    v.d = 0.;
    if( toupper(buf[1]) == 'I' && toupper(buf[2]) == 'N' && toupper(buf[3]) == 'F' )
        v.i = (uint64)inf_hi << 32;
    else if( toupper(buf[1]) == 'N' && toupper(buf[2]) == 'A' && toupper(buf[3]) == 'N' )
        v.i = (uint64)-1;
    else
        CV_PARSE_ERROR( "Bad format of floating-point constant" );
    *value = v.d;

    *endptr = buf + 4;
}


double icv_strtod( CvFileStorage* fs, char* ptr, char** endptr )
{
    double fval = strtod( ptr, endptr );
    if( **endptr == '.' )
    {
        char* dot_pos = *endptr;
        *dot_pos = ',';
        double fval2 = strtod( ptr, endptr );
        *dot_pos = '.';
        if( *endptr > dot_pos )
            fval = fval2;
        else
            *endptr = dot_pos;
    }

    if( *endptr == ptr || cv_isalpha(**endptr) )
        icvProcessSpecialDouble( fs, ptr, &fval, endptr );

    return fval;
}

// this function will convert "aa?bb&cc&dd" to {"aa", "bb", "cc", "dd"}
static std::vector<std::string> analyze_file_name( std::string const & file_name )
{
    static const char not_file_name       = '\n';
    static const char parameter_begin     = '?';
    static const char parameter_separator = '&';
    std::vector<std::string> result;

    if ( file_name.find(not_file_name, 0U) != std::string::npos )
        return result;

    size_t beg = file_name.find_last_of(parameter_begin);
    size_t end = file_name.size();
    result.push_back(file_name.substr(0U, beg));

    if ( beg != std::string::npos )
    {
        beg ++;
        for ( size_t param_beg = beg, param_end = beg;
              param_end < end;
              param_beg = param_end + 1U )
        {
            param_end = file_name.find_first_of( parameter_separator, param_beg );
            if ( (param_end == std::string::npos || param_end != param_beg) && param_beg + 1U < end )
            {
                result.push_back( file_name.substr( param_beg, param_end - param_beg ) );
            }
        }
    }

    return result;
}

static bool is_param_exist( const std::vector<std::string> & params, const std::string & param )
{
    if ( params.size() < 2U )
        return false;

    return std::find(params.begin(), params.end(), param) != params.end();
}

void switch_to_Base64_state( CvFileStorage* fs, base64::fs::State state )
{
    const char * err_unkonwn_state = "Unexpected error, unable to determine the Base64 state.";
    const char * err_unable_to_switch = "Unexpected error, unable to switch to this state.";

    /* like a finite state machine */
    switch (fs->state_of_writing_base64)
    {
    case base64::fs::Uncertain:
        switch (state)
        {
        case base64::fs::InUse:
            CV_DbgAssert( fs->base64_writer == 0 );
            fs->base64_writer = new base64::Base64Writer( fs );
            break;
        case base64::fs::Uncertain:
            break;
        case base64::fs::NotUse:
            break;
        default:
            CV_Error( CV_StsError, err_unkonwn_state );
            break;
        }
        break;
    case base64::fs::InUse:
        switch (state)
        {
        case base64::fs::InUse:
        case base64::fs::NotUse:
            CV_Error( CV_StsError, err_unable_to_switch );
            break;
        case base64::fs::Uncertain:
            delete fs->base64_writer;
            fs->base64_writer = 0;
            break;
        default:
            CV_Error( CV_StsError, err_unkonwn_state );
            break;
        }
        break;
    case base64::fs::NotUse:
        switch (state)
        {
        case base64::fs::InUse:
        case base64::fs::NotUse:
            CV_Error( CV_StsError, err_unable_to_switch );
            break;
        case base64::fs::Uncertain:
            break;
        default:
            CV_Error( CV_StsError, err_unkonwn_state );
            break;
        }
        break;
    default:
        CV_Error( CV_StsError, err_unkonwn_state );
        break;
    }

    fs->state_of_writing_base64 = state;
}


void check_if_write_struct_is_delayed( CvFileStorage* fs, bool change_type_to_base64 )
{
    if ( fs->is_write_struct_delayed )
    {
        /* save data to prevent recursive call errors */
        std::string struct_key;
        std::string type_name;
        int struct_flags = fs->delayed_struct_flags;

        if ( fs->delayed_struct_key != 0 && *fs->delayed_struct_key != '\0' )
        {
            struct_key.assign(fs->delayed_struct_key);
        }
        if ( fs->delayed_type_name != 0 && *fs->delayed_type_name != '\0' )
        {
            type_name.assign(fs->delayed_type_name);
        }

        /* reset */
        delete[] fs->delayed_struct_key;
        delete[] fs->delayed_type_name;
        fs->delayed_struct_key   = 0;
        fs->delayed_struct_flags = 0;
        fs->delayed_type_name    = 0;

        fs->is_write_struct_delayed = false;

        /* call */
        if ( change_type_to_base64 )
        {
            fs->start_write_struct( fs, struct_key.c_str(), struct_flags, "binary");
            if ( fs->state_of_writing_base64 != base64::fs::Uncertain )
                switch_to_Base64_state( fs, base64::fs::Uncertain );
            switch_to_Base64_state( fs, base64::fs::InUse );
        }
        else
        {
            fs->start_write_struct( fs, struct_key.c_str(), struct_flags, type_name.c_str());
            if ( fs->state_of_writing_base64 != base64::fs::Uncertain )
                switch_to_Base64_state( fs, base64::fs::Uncertain );
            switch_to_Base64_state( fs, base64::fs::NotUse );
        }
    }
}


static void make_write_struct_delayed(
    CvFileStorage* fs,
    const char* key,
    int struct_flags,
    const char* type_name )
{
    CV_Assert( fs->is_write_struct_delayed == false );
    CV_DbgAssert( fs->delayed_struct_key   == 0 );
    CV_DbgAssert( fs->delayed_struct_flags == 0 );
    CV_DbgAssert( fs->delayed_type_name    == 0 );

    fs->delayed_struct_flags = struct_flags;

    if ( key != 0 )
    {
        fs->delayed_struct_key = new char[strlen(key) + 1U];
        strcpy(fs->delayed_struct_key, key);
    }

    if ( type_name != 0 )
    {
        fs->delayed_type_name = new char[strlen(type_name) + 1U];
        strcpy(fs->delayed_type_name, type_name);
    }

    fs->is_write_struct_delayed = true;
}





/****************************************************************************************\
*                              Common High-Level Functions                               *
\****************************************************************************************/

CV_IMPL CvFileStorage*
cvOpenFileStorage( const char* query, CvMemStorage* dststorage, int flags, const char* encoding )
{
    CvFileStorage* fs = 0;
    int default_block_size = 1 << 18;
    bool append = (flags & 3) == CV_STORAGE_APPEND;
    bool mem = (flags & CV_STORAGE_MEMORY) != 0;
    bool write_mode = (flags & 3) != 0;
    bool write_base64 = (write_mode || append) && (flags & CV_STORAGE_BASE64) != 0;
    bool isGZ = false;
    size_t fnamelen = 0;
    const char * filename = query;

    std::vector<std::string> params;
    if ( !mem )
    {
        params = analyze_file_name( query );
        if ( !params.empty() )
            filename = params.begin()->c_str();

        if ( write_base64 == false && is_param_exist( params, "base64" ) )
            write_base64 = (write_mode || append);
    }

    if( !filename || filename[0] == '\0' )
    {
        if( !write_mode )
            CV_Error( CV_StsNullPtr, mem ? "NULL or empty filename" : "NULL or empty buffer" );
        mem = true;
    }
    else
        fnamelen = strlen(filename);

    if( mem && append )
        CV_Error( CV_StsBadFlag, "CV_STORAGE_APPEND and CV_STORAGE_MEMORY are not currently compatible" );

    fs = (CvFileStorage*)cvAlloc( sizeof(*fs) );
    CV_Assert(fs);
    memset( fs, 0, sizeof(*fs));

    fs->memstorage = cvCreateMemStorage( default_block_size );
    fs->dststorage = dststorage ? dststorage : fs->memstorage;

    fs->flags = CV_FILE_STORAGE;
    fs->write_mode = write_mode;

    if( !mem )
    {
        fs->filename = (char*)cvMemStorageAlloc( fs->memstorage, fnamelen+1 );
        strcpy( fs->filename, filename );

        char* dot_pos = strrchr(fs->filename, '.');
        char compression = '\0';

        if( dot_pos && dot_pos[1] == 'g' && dot_pos[2] == 'z' &&
            (dot_pos[3] == '\0' || (cv_isdigit(dot_pos[3]) && dot_pos[4] == '\0')) )
        {
            if( append )
            {
                cvReleaseFileStorage( &fs );
                CV_Error(CV_StsNotImplemented, "Appending data to compressed file is not implemented" );
            }
            isGZ = true;
            compression = dot_pos[3];
            if( compression )
                dot_pos[3] = '\0', fnamelen--;
        }

        if( !isGZ )
        {
            fs->file = fopen(fs->filename, !fs->write_mode ? "rt" : !append ? "wt" : "a+t" );
            if( !fs->file )
                goto _exit_;
        }
        else
        {
            #if USE_ZLIB
            char mode[] = { fs->write_mode ? 'w' : 'r', 'b', compression ? compression : '3', '\0' };
            fs->gzfile = gzopen(fs->filename, mode);
            if( !fs->gzfile )
                goto _exit_;
            #else
            cvReleaseFileStorage( &fs );
            CV_Error(CV_StsNotImplemented, "There is no compressed file storage support in this configuration");
            #endif
        }
    }

    fs->roots = 0;
    fs->struct_indent = 0;
    fs->struct_flags = 0;
    fs->wrap_margin = 71;

    if( fs->write_mode )
    {
        int fmt = flags & CV_STORAGE_FORMAT_MASK;

        if( mem )
            fs->outbuf = new std::deque<char>;

        if( fmt == CV_STORAGE_FORMAT_AUTO && filename )
        {
            const char* dot_pos = NULL;
            const char* dot_pos2 = NULL;
            // like strrchr() implementation, but save two last positions simultaneously
            for (const char* pos = filename; pos[0] != 0; pos++)
            {
                if (pos[0] == '.')
                {
                    dot_pos2 = dot_pos;
                    dot_pos = pos;
                }
            }
            if (cv_strcasecmp(dot_pos, ".gz") && dot_pos2 != NULL)
            {
                dot_pos = dot_pos2;
            }
            fs->fmt
                = (cv_strcasecmp(dot_pos, ".xml") || cv_strcasecmp(dot_pos, ".xml.gz"))
                ? CV_STORAGE_FORMAT_XML
                : (cv_strcasecmp(dot_pos, ".json") || cv_strcasecmp(dot_pos, ".json.gz"))
                ? CV_STORAGE_FORMAT_JSON
                : CV_STORAGE_FORMAT_YAML
                ;
        }
        else if ( fmt != CV_STORAGE_FORMAT_AUTO )
        {
            fs->fmt = fmt;
        }
        else
        {
            fs->fmt = CV_STORAGE_FORMAT_XML;
        }

        // we use factor=6 for XML (the longest characters (' and ") are encoded with 6 bytes (&apos; and &quot;)
        // and factor=4 for YAML ( as we use 4 bytes for non ASCII characters (e.g. \xAB))
        int buf_size = CV_FS_MAX_LEN*(fs->fmt == CV_STORAGE_FORMAT_XML ? 6 : 4) + 1024;

        if( append )
            fseek( fs->file, 0, SEEK_END );

        fs->write_stack = cvCreateSeq( 0, sizeof(CvSeq), fs->fmt == CV_STORAGE_FORMAT_XML ?
                sizeof(CvXMLStackRecord) : sizeof(int), fs->memstorage );
        fs->is_first = 1;
        fs->struct_indent = 0;
        fs->struct_flags = CV_NODE_EMPTY;
        fs->buffer_start = fs->buffer = (char*)cvAlloc( buf_size + 1024 );
        fs->buffer_end = fs->buffer_start + buf_size;

        fs->base64_writer           = 0;
        fs->is_default_using_base64 = write_base64;
        fs->state_of_writing_base64 = base64::fs::Uncertain;

        fs->is_write_struct_delayed = false;
        fs->delayed_struct_key      = 0;
        fs->delayed_struct_flags    = 0;
        fs->delayed_type_name       = 0;

        if( fs->fmt == CV_STORAGE_FORMAT_XML )
        {
            size_t file_size = fs->file ? (size_t)ftell( fs->file ) : (size_t)0;
            fs->strstorage = cvCreateChildMemStorage( fs->memstorage );
            if( !append || file_size == 0 )
            {
                if( encoding )
                {
                    if( strcmp( encoding, "UTF-16" ) == 0 ||
                        strcmp( encoding, "utf-16" ) == 0 ||
                        strcmp( encoding, "Utf-16" ) == 0 )
                    {
                        cvReleaseFileStorage( &fs );
                        CV_Error( CV_StsBadArg, "UTF-16 XML encoding is not supported! Use 8-bit encoding\n");
                    }

                    CV_Assert( strlen(encoding) < 1000 );
                    char buf[1100];
                    sprintf(buf, "<?xml version=\"1.0\" encoding=\"%s\"?>\n", encoding);
                    icvPuts( fs, buf );
                }
                else
                    icvPuts( fs, "<?xml version=\"1.0\"?>\n" );
                icvPuts( fs, "<opencv_storage>\n" );
            }
            else
            {
                int xml_buf_size = 1 << 10;
                char substr[] = "</opencv_storage>";
                int last_occurence = -1;
                xml_buf_size = MIN(xml_buf_size, int(file_size));
                fseek( fs->file, -xml_buf_size, SEEK_END );
                char* xml_buf = (char*)cvAlloc( xml_buf_size+2 );
                // find the last occurence of </opencv_storage>
                for(;;)
                {
                    int line_offset = (int)ftell( fs->file );
                    char* ptr0 = icvGets( fs, xml_buf, xml_buf_size ), *ptr;
                    if( !ptr0 )
                        break;
                    ptr = ptr0;
                    for(;;)
                    {
                        ptr = strstr( ptr, substr );
                        if( !ptr )
                            break;
                        last_occurence = line_offset + (int)(ptr - ptr0);
                        ptr += strlen(substr);
                    }
                }
                cvFree( &xml_buf );
                if( last_occurence < 0 )
                {
                    cvReleaseFileStorage( &fs );
                    CV_Error( CV_StsError, "Could not find </opencv_storage> in the end of file.\n" );
                }
                icvCloseFile( fs );
                fs->file = fopen( fs->filename, "r+t" );
                CV_Assert(fs->file);
                fseek( fs->file, last_occurence, SEEK_SET );
                // replace the last "</opencv_storage>" with " <!-- resumed -->", which has the same length
                icvPuts( fs, " <!-- resumed -->" );
                fseek( fs->file, 0, SEEK_END );
                icvPuts( fs, "\n" );
            }
            fs->start_write_struct = icvXMLStartWriteStruct;
            fs->end_write_struct = icvXMLEndWriteStruct;
            fs->write_int = icvXMLWriteInt;
            fs->write_real = icvXMLWriteReal;
            fs->write_string = icvXMLWriteString;
            fs->write_comment = icvXMLWriteComment;
            fs->start_next_stream = icvXMLStartNextStream;
        }
        else if( fs->fmt == CV_STORAGE_FORMAT_YAML )
        {
            if( !append )
                icvPuts( fs, "%YAML:1.0\n---\n" );
            else
                icvPuts( fs, "...\n---\n" );
            fs->start_write_struct = icvYMLStartWriteStruct;
            fs->end_write_struct = icvYMLEndWriteStruct;
            fs->write_int = icvYMLWriteInt;
            fs->write_real = icvYMLWriteReal;
            fs->write_string = icvYMLWriteString;
            fs->write_comment = icvYMLWriteComment;
            fs->start_next_stream = icvYMLStartNextStream;
        }
        else
        {
            if( !append )
                icvPuts( fs, "{\n" );
            else
            {
                bool valid = false;
                long roffset = 0;
                for ( ;
                      fseek( fs->file, roffset, SEEK_END ) == 0;
                      roffset -= 1 )
                {
                    const char end_mark = '}';
                    if ( fgetc( fs->file ) == end_mark )
                    {
                        fseek( fs->file, roffset, SEEK_END );
                        valid = true;
                        break;
                    }
                }

                if ( valid )
                {
                    icvCloseFile( fs );
                    fs->file = fopen( fs->filename, "r+t" );
                    CV_Assert(fs->file);
                    fseek( fs->file, roffset, SEEK_END );
                    fputs( ",", fs->file );
                }
                else
                {
                    CV_Error( CV_StsError, "Could not find '}' in the end of file.\n" );
                }
            }
            fs->struct_indent = 4;
            fs->start_write_struct = icvJSONStartWriteStruct;
            fs->end_write_struct = icvJSONEndWriteStruct;
            fs->write_int = icvJSONWriteInt;
            fs->write_real = icvJSONWriteReal;
            fs->write_string = icvJSONWriteString;
            fs->write_comment = icvJSONWriteComment;
            fs->start_next_stream = icvJSONStartNextStream;
        }
    }
    else
    {
        if( mem )
        {
            fs->strbuf = filename;
            fs->strbufsize = fnamelen;
        }

        size_t buf_size = 1 << 20;
        const char* yaml_signature = "%YAML";
        const char* json_signature = "{";
        const char* xml_signature  = "<?xml";
        char buf[16];
        icvGets( fs, buf, sizeof(buf)-2 );
        char* bufPtr = cv_skip_BOM(buf);
        size_t bufOffset = bufPtr - buf;

        if(strncmp( bufPtr, yaml_signature, strlen(yaml_signature) ) == 0)
            fs->fmt = CV_STORAGE_FORMAT_YAML;
        else if(strncmp( bufPtr, json_signature, strlen(json_signature) ) == 0)
            fs->fmt = CV_STORAGE_FORMAT_JSON;
        else if(strncmp( bufPtr, xml_signature, strlen(xml_signature) ) == 0)
            fs->fmt = CV_STORAGE_FORMAT_XML;
        else if(fs->strbufsize  == bufOffset)
            CV_Error(CV_BADARG_ERR, "Input file is empty");
        else
            CV_Error(CV_BADARG_ERR, "Unsupported file storage format");

        if( !isGZ )
        {
            if( !mem )
            {
                fseek( fs->file, 0, SEEK_END );
                buf_size = ftell( fs->file );
            }
            else
                buf_size = fs->strbufsize;
            buf_size = MIN( buf_size, (size_t)(1 << 20) );
            buf_size = MAX( buf_size, (size_t)(CV_FS_MAX_LEN*2 + 1024) );
        }
        icvRewind(fs);
        fs->strbufpos = bufOffset;

        fs->str_hash = cvCreateMap( 0, sizeof(CvStringHash),
                        sizeof(CvStringHashNode), fs->memstorage, 256 );

        fs->roots = cvCreateSeq( 0, sizeof(CvSeq),
                        sizeof(CvFileNode), fs->memstorage );

        fs->buffer = fs->buffer_start = (char*)cvAlloc( buf_size + 256 );
        fs->buffer_end = fs->buffer_start + buf_size;
        fs->buffer[0] = '\n';
        fs->buffer[1] = '\0';

        //mode = cvGetErrMode();
        //cvSetErrMode( CV_ErrModeSilent );
        CV_TRY
        {
            switch (fs->fmt)
            {
            case CV_STORAGE_FORMAT_XML : { icvXMLParse ( fs ); break; }
            case CV_STORAGE_FORMAT_YAML: { icvYMLParse ( fs ); break; }
            case CV_STORAGE_FORMAT_JSON: { icvJSONParse( fs ); break; }
            default: break;
            }
        }
        CV_CATCH_ALL
        {
            fs->is_opened = true;
            cvReleaseFileStorage( &fs );
            CV_RETHROW();
        }
        //cvSetErrMode( mode );

        // release resources that we do not need anymore
        cvFree( &fs->buffer_start );
        fs->buffer = fs->buffer_end = 0;
    }
    fs->is_opened = true;

_exit_:
    if( fs )
    {
        if( cvGetErrStatus() < 0 || (!fs->file && !fs->gzfile && !fs->outbuf && !fs->strbuf) )
        {
            cvReleaseFileStorage( &fs );
        }
        else if( !fs->write_mode )
        {
            icvCloseFile(fs);
            // we close the file since it's not needed anymore. But icvCloseFile() resets is_opened,
            // which may be misleading. Since we restore the value of is_opened.
            fs->is_opened = true;
        }
    }

    return  fs;
}


CV_IMPL void
cvStartWriteStruct( CvFileStorage* fs, const char* key, int struct_flags,
                    const char* type_name, CvAttrList /*attributes*/ )
{
    CV_CHECK_OUTPUT_FILE_STORAGE(fs);
    check_if_write_struct_is_delayed( fs );
    if ( fs->state_of_writing_base64 == base64::fs::NotUse )
        switch_to_Base64_state( fs, base64::fs::Uncertain );

    if ( fs->state_of_writing_base64 == base64::fs::Uncertain
        &&
        CV_NODE_IS_SEQ(struct_flags)
        &&
        fs->is_default_using_base64
        &&
        type_name == 0
       )
    {
        /* Uncertain whether output Base64 data */
        make_write_struct_delayed( fs, key, struct_flags, type_name );
    }
    else if ( type_name && memcmp(type_name, "binary", 6) == 0 )
    {
        /* Must output Base64 data */
        if ( !CV_NODE_IS_SEQ(struct_flags) )
            CV_Error( CV_StsBadArg, "must set 'struct_flags |= CV_NODE_SEQ' if using Base64.");
        else if ( fs->state_of_writing_base64 != base64::fs::Uncertain )
            CV_Error( CV_StsError, "function \'cvStartWriteStruct\' calls cannot be nested if using Base64.");

        fs->start_write_struct( fs, key, struct_flags, type_name );

        if ( fs->state_of_writing_base64 != base64::fs::Uncertain )
            switch_to_Base64_state( fs, base64::fs::Uncertain );
        switch_to_Base64_state( fs, base64::fs::InUse );
    }
    else
    {
        /* Won't output Base64 data */
        if ( fs->state_of_writing_base64 == base64::fs::InUse )
            CV_Error( CV_StsError, "At the end of the output Base64, `cvEndWriteStruct` is needed.");

        fs->start_write_struct( fs, key, struct_flags, type_name );

        if ( fs->state_of_writing_base64 != base64::fs::Uncertain )
            switch_to_Base64_state( fs, base64::fs::Uncertain );
        switch_to_Base64_state( fs, base64::fs::NotUse );
    }
}


CV_IMPL void
cvEndWriteStruct( CvFileStorage* fs )
{
    CV_CHECK_OUTPUT_FILE_STORAGE(fs);
    check_if_write_struct_is_delayed( fs );

    if ( fs->state_of_writing_base64 != base64::fs::Uncertain )
        switch_to_Base64_state( fs, base64::fs::Uncertain );

    fs->end_write_struct( fs );
}


CV_IMPL void
cvWriteInt( CvFileStorage* fs, const char* key, int value )
{
    CV_CHECK_OUTPUT_FILE_STORAGE(fs);
    fs->write_int( fs, key, value );
}


CV_IMPL void
cvWriteReal( CvFileStorage* fs, const char* key, double value )
{
    CV_CHECK_OUTPUT_FILE_STORAGE(fs);
    fs->write_real( fs, key, value );
}


CV_IMPL void
cvWriteString( CvFileStorage* fs, const char* key, const char* value, int quote )
{
    CV_CHECK_OUTPUT_FILE_STORAGE(fs);
    fs->write_string( fs, key, value, quote );
}


CV_IMPL void
cvWriteComment( CvFileStorage* fs, const char* comment, int eol_comment )
{
    CV_CHECK_OUTPUT_FILE_STORAGE(fs);
    fs->write_comment( fs, comment, eol_comment );
}


CV_IMPL void
cvStartNextStream( CvFileStorage* fs )
{
    CV_CHECK_OUTPUT_FILE_STORAGE(fs);
    fs->start_next_stream( fs );
}


static const char icvTypeSymbol[] = "ucwsifdr";
#define CV_FS_MAX_FMT_PAIRS  128

static char*
icvEncodeFormat( int elem_type, char* dt )
{
    sprintf( dt, "%d%c", CV_MAT_CN(elem_type), icvTypeSymbol[CV_MAT_DEPTH(elem_type)] );
    return dt + ( dt[2] == '\0' && dt[0] == '1' );
}

static int
icvDecodeFormat( const char* dt, int* fmt_pairs, int max_len )
{
    int fmt_pair_count = 0;
    int i = 0, k = 0, len = dt ? (int)strlen(dt) : 0;

    if( !dt || !len )
        return 0;

    assert( fmt_pairs != 0 && max_len > 0 );
    fmt_pairs[0] = 0;
    max_len *= 2;

    for( ; k < len; k++ )
    {
        char c = dt[k];

        if( cv_isdigit(c) )
        {
            int count = c - '0';
            if( cv_isdigit(dt[k+1]) )
            {
                char* endptr = 0;
                count = (int)strtol( dt+k, &endptr, 10 );
                k = (int)(endptr - dt) - 1;
            }

            if( count <= 0 )
                CV_Error( CV_StsBadArg, "Invalid data type specification" );

            fmt_pairs[i] = count;
        }
        else
        {
            const char* pos = strchr( icvTypeSymbol, c );
            if( !pos )
                CV_Error( CV_StsBadArg, "Invalid data type specification" );
            if( fmt_pairs[i] == 0 )
                fmt_pairs[i] = 1;
            fmt_pairs[i+1] = (int)(pos - icvTypeSymbol);
            if( i > 0 && fmt_pairs[i+1] == fmt_pairs[i-1] )
                fmt_pairs[i-2] += fmt_pairs[i];
            else
            {
                i += 2;
                if( i >= max_len )
                    CV_Error( CV_StsBadArg, "Too long data type specification" );
            }
            fmt_pairs[i] = 0;
        }
    }

    fmt_pair_count = i/2;
    return fmt_pair_count;
}


static int
icvCalcElemSize( const char* dt, int initial_size )
{
    int size = 0;
    int fmt_pairs[CV_FS_MAX_FMT_PAIRS], i, fmt_pair_count;
    int comp_size;

    fmt_pair_count = icvDecodeFormat( dt, fmt_pairs, CV_FS_MAX_FMT_PAIRS );
    fmt_pair_count *= 2;
    for( i = 0, size = initial_size; i < fmt_pair_count; i += 2 )
    {
        comp_size = CV_ELEM_SIZE(fmt_pairs[i+1]);
        size = cvAlign( size, comp_size );
        size += comp_size * fmt_pairs[i];
    }
    if( initial_size == 0 )
    {
        comp_size = CV_ELEM_SIZE(fmt_pairs[1]);
        size = cvAlign( size, comp_size );
    }
    return size;
}


int icvCalcStructSize( const char* dt, int initial_size )
{
    int size = icvCalcElemSize( dt, initial_size );
    size_t elem_max_size = 0;
    for ( const char * type = dt; *type != '\0'; type++ ) {
        switch ( *type )
        {
        case 'u': { elem_max_size = std::max( elem_max_size, sizeof(uchar ) ); break; }
        case 'c': { elem_max_size = std::max( elem_max_size, sizeof(schar ) ); break; }
        case 'w': { elem_max_size = std::max( elem_max_size, sizeof(ushort) ); break; }
        case 's': { elem_max_size = std::max( elem_max_size, sizeof(short ) ); break; }
        case 'i': { elem_max_size = std::max( elem_max_size, sizeof(int   ) ); break; }
        case 'f': { elem_max_size = std::max( elem_max_size, sizeof(float ) ); break; }
        case 'd': { elem_max_size = std::max( elem_max_size, sizeof(double) ); break; }
        default: break;
        }
    }
    size = cvAlign( size, static_cast<int>(elem_max_size) );
    return size;
}


static int
icvDecodeSimpleFormat( const char* dt )
{
    int elem_type = -1;
    int fmt_pairs[CV_FS_MAX_FMT_PAIRS], fmt_pair_count;

    fmt_pair_count = icvDecodeFormat( dt, fmt_pairs, CV_FS_MAX_FMT_PAIRS );
    if( fmt_pair_count != 1 || fmt_pairs[0] >= CV_CN_MAX)
        CV_Error( CV_StsError, "Too complex format for the matrix" );

    elem_type = CV_MAKETYPE( fmt_pairs[1], fmt_pairs[0] );

    return elem_type;
}


CV_IMPL void
cvWriteRawData( CvFileStorage* fs, const void* _data, int len, const char* dt )
{
    if (fs->is_default_using_base64 ||
        fs->state_of_writing_base64 == base64::fs::InUse )
    {
        base64::cvWriteRawDataBase64( fs, _data, len, dt );
        return;
    }
    else if ( fs->state_of_writing_base64 == base64::fs::Uncertain )
    {
        switch_to_Base64_state( fs, base64::fs::NotUse );
    }

    const char* data0 = (const char*)_data;
    int offset = 0;
    int fmt_pairs[CV_FS_MAX_FMT_PAIRS*2], k, fmt_pair_count;
    char buf[256] = "";

    CV_CHECK_OUTPUT_FILE_STORAGE( fs );

    if( len < 0 )
        CV_Error( CV_StsOutOfRange, "Negative number of elements" );

    fmt_pair_count = icvDecodeFormat( dt, fmt_pairs, CV_FS_MAX_FMT_PAIRS );

    if( !len )
        return;

    if( !data0 )
        CV_Error( CV_StsNullPtr, "Null data pointer" );

    if( fmt_pair_count == 1 )
    {
        fmt_pairs[0] *= len;
        len = 1;
    }

    for(;len--;)
    {
        for( k = 0; k < fmt_pair_count; k++ )
        {
            int i, count = fmt_pairs[k*2];
            int elem_type = fmt_pairs[k*2+1];
            int elem_size = CV_ELEM_SIZE(elem_type);
            const char* data, *ptr;

            offset = cvAlign( offset, elem_size );
            data = data0 + offset;

            for( i = 0; i < count; i++ )
            {
                switch( elem_type )
                {
                case CV_8U:
                    ptr = icv_itoa( *(uchar*)data, buf, 10 );
                    data++;
                    break;
                case CV_8S:
                    ptr = icv_itoa( *(char*)data, buf, 10 );
                    data++;
                    break;
                case CV_16U:
                    ptr = icv_itoa( *(ushort*)data, buf, 10 );
                    data += sizeof(ushort);
                    break;
                case CV_16S:
                    ptr = icv_itoa( *(short*)data, buf, 10 );
                    data += sizeof(short);
                    break;
                case CV_32S:
                    ptr = icv_itoa( *(int*)data, buf, 10 );
                    data += sizeof(int);
                    break;
                case CV_32F:
                    ptr = icvFloatToString( buf, *(float*)data );
                    data += sizeof(float);
                    break;
                case CV_64F:
                    ptr = icvDoubleToString( buf, *(double*)data );
                    data += sizeof(double);
                    break;
                case CV_USRTYPE1: /* reference */
                    ptr = icv_itoa( (int)*(size_t*)data, buf, 10 );
                    data += sizeof(size_t);
                    break;
                default:
                    CV_Error( CV_StsUnsupportedFormat, "Unsupported type" );
                    return;
                }

                if( fs->fmt == CV_STORAGE_FORMAT_XML )
                {
                    int buf_len = (int)strlen(ptr);
                    icvXMLWriteScalar( fs, 0, ptr, buf_len );
                }
                else if ( fs->fmt == CV_STORAGE_FORMAT_YAML )
                {
                    icvYMLWrite( fs, 0, ptr );
                }
                else
                {
                    if( elem_type == CV_32F || elem_type == CV_64F )
                    {
                        size_t buf_len = strlen(ptr);
                        if( buf_len > 0 && ptr[buf_len-1] == '.' )
                        {
                            // append zero if CV_32F or CV_64F string ends with decimal place to match JSON standard
                            // ptr will point to buf, so can write to buf given ptr is const
                            buf[buf_len] = '0';
                            buf[buf_len+1] = '\0';
                        }
                    }
                    icvJSONWrite( fs, 0, ptr );
                }
            }

            offset = (int)(data - data0);
        }
    }
}


CV_IMPL void
cvStartReadRawData( const CvFileStorage* fs, const CvFileNode* src, CvSeqReader* reader )
{
    int node_type;
    CV_CHECK_FILE_STORAGE( fs );

    if( !src || !reader )
        CV_Error( CV_StsNullPtr, "Null pointer to source file node or reader" );

    node_type = CV_NODE_TYPE(src->tag);
    if( node_type == CV_NODE_INT || node_type == CV_NODE_REAL )
    {
        // emulate reading from 1-element sequence
        reader->ptr = (schar*)src;
        reader->block_max = reader->ptr + sizeof(*src)*2;
        reader->block_min = reader->ptr;
        reader->seq = 0;
    }
    else if( node_type == CV_NODE_SEQ )
    {
        cvStartReadSeq( src->data.seq, reader, 0 );
    }
    else if( node_type == CV_NODE_NONE )
    {
        memset( reader, 0, sizeof(*reader) );
    }
    else
        CV_Error( CV_StsBadArg, "The file node should be a numerical scalar or a sequence" );
}


CV_IMPL void
cvReadRawDataSlice( const CvFileStorage* fs, CvSeqReader* reader,
                    int len, void* _data, const char* dt )
{
    char* data0 = (char*)_data;
    int fmt_pairs[CV_FS_MAX_FMT_PAIRS*2], k = 0, fmt_pair_count;
    int i = 0, count = 0;

    CV_CHECK_FILE_STORAGE( fs );

    if( !reader || !data0 )
        CV_Error( CV_StsNullPtr, "Null pointer to reader or destination array" );

    if( !reader->seq && len != 1 )
        CV_Error( CV_StsBadSize, "The readed sequence is a scalar, thus len must be 1" );

    fmt_pair_count = icvDecodeFormat( dt, fmt_pairs, CV_FS_MAX_FMT_PAIRS );
    size_t step = ::icvCalcStructSize(dt, 0);

    for(;;)
    {
        int offset = 0;
        for( k = 0; k < fmt_pair_count; k++ )
        {
            int elem_type = fmt_pairs[k*2+1];
            int elem_size = CV_ELEM_SIZE(elem_type);
            char* data;

            count = fmt_pairs[k*2];
            offset = cvAlign( offset, elem_size );
            data = data0 + offset;

            for( i = 0; i < count; i++ )
            {
                CvFileNode* node = (CvFileNode*)reader->ptr;
                if( CV_NODE_IS_INT(node->tag) )
                {
                    int ival = node->data.i;

                    switch( elem_type )
                    {
                    case CV_8U:
                        *(uchar*)data = cv::saturate_cast<uchar>(ival);
                        data++;
                        break;
                    case CV_8S:
                        *(char*)data = cv::saturate_cast<schar>(ival);
                        data++;
                        break;
                    case CV_16U:
                        *(ushort*)data = cv::saturate_cast<ushort>(ival);
                        data += sizeof(ushort);
                        break;
                    case CV_16S:
                        *(short*)data = cv::saturate_cast<short>(ival);
                        data += sizeof(short);
                        break;
                    case CV_32S:
                        *(int*)data = ival;
                        data += sizeof(int);
                        break;
                    case CV_32F:
                        *(float*)data = (float)ival;
                        data += sizeof(float);
                        break;
                    case CV_64F:
                        *(double*)data = (double)ival;
                        data += sizeof(double);
                        break;
                    case CV_USRTYPE1: /* reference */
                        *(size_t*)data = ival;
                        data += sizeof(size_t);
                        break;
                    default:
                        CV_Error( CV_StsUnsupportedFormat, "Unsupported type" );
                        return;
                    }
                }
                else if( CV_NODE_IS_REAL(node->tag) )
                {
                    double fval = node->data.f;
                    int ival;

                    switch( elem_type )
                    {
                    case CV_8U:
                        ival = cvRound(fval);
                        *(uchar*)data = cv::saturate_cast<uchar>(ival);
                        data++;
                        break;
                    case CV_8S:
                        ival = cvRound(fval);
                        *(char*)data = cv::saturate_cast<schar>(ival);
                        data++;
                        break;
                    case CV_16U:
                        ival = cvRound(fval);
                        *(ushort*)data = cv::saturate_cast<ushort>(ival);
                        data += sizeof(ushort);
                        break;
                    case CV_16S:
                        ival = cvRound(fval);
                        *(short*)data = cv::saturate_cast<short>(ival);
                        data += sizeof(short);
                        break;
                    case CV_32S:
                        ival = cvRound(fval);
                        *(int*)data = ival;
                        data += sizeof(int);
                        break;
                    case CV_32F:
                        *(float*)data = (float)fval;
                        data += sizeof(float);
                        break;
                    case CV_64F:
                        *(double*)data = fval;
                        data += sizeof(double);
                        break;
                    case CV_USRTYPE1: /* reference */
                        ival = cvRound(fval);
                        *(size_t*)data = ival;
                        data += sizeof(size_t);
                        break;
                    default:
                        CV_Error( CV_StsUnsupportedFormat, "Unsupported type" );
                        return;
                    }
                }
                else
                    CV_Error( CV_StsError,
                    "The sequence element is not a numerical scalar" );

                CV_NEXT_SEQ_ELEM( sizeof(CvFileNode), *reader );
                if( !--len )
                    goto end_loop;
            }

            offset = (int)(data - data0);
        }
        data0 += step;
    }

end_loop:
    if( i != count - 1 || k != fmt_pair_count - 1 )
        CV_Error( CV_StsBadSize,
        "The sequence slice does not fit an integer number of records" );

    if( !reader->seq )
        reader->ptr -= sizeof(CvFileNode);
}


CV_IMPL void
cvReadRawData( const CvFileStorage* fs, const CvFileNode* src,
               void* data, const char* dt )
{
    CvSeqReader reader;

    if( !src || !data )
        CV_Error( CV_StsNullPtr, "Null pointers to source file node or destination array" );

    cvStartReadRawData( fs, src, &reader );
    cvReadRawDataSlice( fs, &reader, CV_NODE_IS_SEQ(src->tag) ?
                        src->data.seq->total : 1, data, dt );
}


static void
icvWriteFileNode( CvFileStorage* fs, const char* name, const CvFileNode* node );

static void
icvWriteCollection( CvFileStorage* fs, const CvFileNode* node )
{
    int i, total = node->data.seq->total;
    int elem_size = node->data.seq->elem_size;
    int is_map = CV_NODE_IS_MAP(node->tag);
    CvSeqReader reader;

    cvStartReadSeq( node->data.seq, &reader, 0 );

    for( i = 0; i < total; i++ )
    {
        CvFileMapNode* elem = (CvFileMapNode*)reader.ptr;
        if( !is_map || CV_IS_SET_ELEM(elem) )
        {
            const char* name = is_map ? elem->key->str.ptr : 0;
            icvWriteFileNode( fs, name, &elem->value );
        }
        CV_NEXT_SEQ_ELEM( elem_size, reader );
    }
}

static void
icvWriteFileNode( CvFileStorage* fs, const char* name, const CvFileNode* node )
{
    switch( CV_NODE_TYPE(node->tag) )
    {
    case CV_NODE_INT:
        fs->write_int( fs, name, node->data.i );
        break;
    case CV_NODE_REAL:
        fs->write_real( fs, name, node->data.f );
        break;
    case CV_NODE_STR:
        fs->write_string( fs, name, node->data.str.ptr, 0 );
        break;
    case CV_NODE_SEQ:
    case CV_NODE_MAP:
        cvStartWriteStruct( fs, name, CV_NODE_TYPE(node->tag) +
                (CV_NODE_SEQ_IS_SIMPLE(node->data.seq) ? CV_NODE_FLOW : 0),
                node->info ? node->info->type_name : 0 );
        icvWriteCollection( fs, node );
        cvEndWriteStruct( fs );
        break;
    case CV_NODE_NONE:
        cvStartWriteStruct( fs, name, CV_NODE_SEQ, 0 );
        cvEndWriteStruct( fs );
        break;
    default:
        CV_Error( CV_StsBadFlag, "Unknown type of file node" );
    }
}


CV_IMPL void
cvWriteFileNode( CvFileStorage* fs, const char* new_node_name,
                 const CvFileNode* node, int embed )
{
    CvFileStorage* dst = 0;
    CV_CHECK_OUTPUT_FILE_STORAGE(fs);

    if( !node )
        return;

    if( CV_NODE_IS_COLLECTION(node->tag) && embed )
    {
        icvWriteCollection( fs, node );
    }
    else
    {
        icvWriteFileNode( fs, new_node_name, node );
    }
    /*
    int i, stream_count;
    stream_count = fs->roots->total;
    for( i = 0; i < stream_count; i++ )
    {
        CvFileNode* node = (CvFileNode*)cvGetSeqElem( fs->roots, i, 0 );
        icvDumpCollection( dst, node );
        if( i < stream_count - 1 )
            dst->start_next_stream( dst );
    }*/
    cvReleaseFileStorage( &dst );
}


CV_IMPL const char*
cvGetFileNodeName( const CvFileNode* file_node )
{
    return file_node && CV_NODE_HAS_NAME(file_node->tag) ?
        ((CvFileMapNode*)file_node)->key->str.ptr : 0;
}

/****************************************************************************************\
*                          Reading/Writing etc. for standard types                       *
\****************************************************************************************/

/*#define CV_TYPE_NAME_MAT "opencv-matrix"
#define CV_TYPE_NAME_MATND "opencv-nd-matrix"
#define CV_TYPE_NAME_SPARSE_MAT "opencv-sparse-matrix"
#define CV_TYPE_NAME_IMAGE "opencv-image"
#define CV_TYPE_NAME_SEQ "opencv-sequence"
#define CV_TYPE_NAME_SEQ_TREE "opencv-sequence-tree"
#define CV_TYPE_NAME_GRAPH "opencv-graph"*/

/******************************* CvMat ******************************/

static int
icvIsMat( const void* ptr )
{
    return CV_IS_MAT_HDR_Z(ptr);
}

static void
icvWriteMat( CvFileStorage* fs, const char* name,
             const void* struct_ptr, CvAttrList /*attr*/ )
{
    const CvMat* mat = (const CvMat*)struct_ptr;
    char dt[16];
    CvSize size;
    int y;

    assert( CV_IS_MAT_HDR_Z(mat) );

    cvStartWriteStruct( fs, name, CV_NODE_MAP, CV_TYPE_NAME_MAT );
    cvWriteInt( fs, "rows", mat->rows );
    cvWriteInt( fs, "cols", mat->cols );
    cvWriteString( fs, "dt", icvEncodeFormat( CV_MAT_TYPE(mat->type), dt ), 0 );
    cvStartWriteStruct( fs, "data", CV_NODE_SEQ + CV_NODE_FLOW );

    size = cvGetSize(mat);
    if( size.height > 0 && size.width > 0 && mat->data.ptr )
    {
        if( CV_IS_MAT_CONT(mat->type) )
        {
            size.width *= size.height;
            size.height = 1;
        }

        for( y = 0; y < size.height; y++ )
            cvWriteRawData( fs, mat->data.ptr + (size_t)y*mat->step, size.width, dt );
    }
    cvEndWriteStruct( fs );
    cvEndWriteStruct( fs );
}


static int
icvFileNodeSeqLen( CvFileNode* node )
{
    return CV_NODE_IS_COLLECTION(node->tag) ? node->data.seq->total :
        CV_NODE_TYPE(node->tag) != CV_NODE_NONE;
}


static void*
icvReadMat( CvFileStorage* fs, CvFileNode* node )
{
    void* ptr = 0;
    CvMat* mat;
    const char* dt;
    CvFileNode* data;
    int rows, cols, elem_type;

    rows = cvReadIntByName( fs, node, "rows", -1 );
    cols = cvReadIntByName( fs, node, "cols", -1 );
    dt = cvReadStringByName( fs, node, "dt", 0 );

    if( rows < 0 || cols < 0 || !dt )
        CV_Error( CV_StsError, "Some of essential matrix attributes are absent" );

    elem_type = icvDecodeSimpleFormat( dt );

    data = cvGetFileNodeByName( fs, node, "data" );
    if( !data )
        CV_Error( CV_StsError, "The matrix data is not found in file storage" );

    int nelems = icvFileNodeSeqLen( data );
    if( nelems > 0 && nelems != rows*cols*CV_MAT_CN(elem_type) )
        CV_Error( CV_StsUnmatchedSizes,
                 "The matrix size does not match to the number of stored elements" );

    if( nelems > 0 )
    {
        mat = cvCreateMat( rows, cols, elem_type );
        cvReadRawData( fs, data, mat->data.ptr, dt );
    }
    else
        mat = cvCreateMatHeader( rows, cols, elem_type );

    ptr = mat;
    return ptr;
}


/******************************* CvMatND ******************************/

static int
icvIsMatND( const void* ptr )
{
    return CV_IS_MATND_HDR(ptr);
}


static void
icvWriteMatND( CvFileStorage* fs, const char* name,
               const void* struct_ptr, CvAttrList /*attr*/ )
{
    CvMatND* mat = (CvMatND*)struct_ptr;
    CvMatND stub;
    CvNArrayIterator iterator;
    int dims, sizes[CV_MAX_DIM];
    char dt[16];

    assert( CV_IS_MATND_HDR(mat) );

    cvStartWriteStruct( fs, name, CV_NODE_MAP, CV_TYPE_NAME_MATND );
    dims = cvGetDims( mat, sizes );
    cvStartWriteStruct( fs, "sizes", CV_NODE_SEQ + CV_NODE_FLOW );
    cvWriteRawData( fs, sizes, dims, "i" );
    cvEndWriteStruct( fs );
    cvWriteString( fs, "dt", icvEncodeFormat( cvGetElemType(mat), dt ), 0 );
    cvStartWriteStruct( fs, "data", CV_NODE_SEQ + CV_NODE_FLOW );

    if( mat->dim[0].size > 0 && mat->data.ptr )
    {
        cvInitNArrayIterator( 1, (CvArr**)&mat, 0, &stub, &iterator );

        do
            cvWriteRawData( fs, iterator.ptr[0], iterator.size.width, dt );
        while( cvNextNArraySlice( &iterator ));
    }
    cvEndWriteStruct( fs );
    cvEndWriteStruct( fs );
}


static void*
icvReadMatND( CvFileStorage* fs, CvFileNode* node )
{
    void* ptr = 0;
    CvMatND* mat;
    const char* dt;
    CvFileNode* data;
    CvFileNode* sizes_node;
    int sizes[CV_MAX_DIM] = {0}, dims, elem_type;
    int i, total_size;

    sizes_node = cvGetFileNodeByName( fs, node, "sizes" );
    dt = cvReadStringByName( fs, node, "dt", 0 );

    if( !sizes_node || !dt )
        CV_Error( CV_StsError, "Some of essential matrix attributes are absent" );

    dims = CV_NODE_IS_SEQ(sizes_node->tag) ? sizes_node->data.seq->total :
           CV_NODE_IS_INT(sizes_node->tag) ? 1 : -1;

    if( dims <= 0 || dims > CV_MAX_DIM )
        CV_Error( CV_StsParseError, "Could not determine the matrix dimensionality" );

    cvReadRawData( fs, sizes_node, sizes, "i" );
    elem_type = icvDecodeSimpleFormat( dt );

    data = cvGetFileNodeByName( fs, node, "data" );
    if( !data )
        CV_Error( CV_StsError, "The matrix data is not found in file storage" );



    for( total_size = CV_MAT_CN(elem_type), i = 0; i < dims; i++ )
    {
        CV_Assert(sizes[i]);
        total_size *= sizes[i];
    }

    int nelems = icvFileNodeSeqLen( data );

    if( nelems > 0 && nelems != total_size )
        CV_Error( CV_StsUnmatchedSizes,
                 "The matrix size does not match to the number of stored elements" );

    if( nelems > 0 )
    {
        mat = cvCreateMatND( dims, sizes, elem_type );
        cvReadRawData( fs, data, mat->data.ptr, dt );
    }
    else
        mat = cvCreateMatNDHeader( dims, sizes, elem_type );

    ptr = mat;
    return ptr;
}


/******************************* CvSparseMat ******************************/

static int
icvIsSparseMat( const void* ptr )
{
    return CV_IS_SPARSE_MAT(ptr);
}


static int
icvSortIdxCmpFunc( const void* _a, const void* _b, void* userdata )
{
    int i, dims = *(int*)userdata;
    const int* a = *(const int**)_a;
    const int* b = *(const int**)_b;

    for( i = 0; i < dims; i++ )
    {
        int delta = a[i] - b[i];
        if( delta )
            return delta;
    }

    return 0;
}


static void
icvWriteSparseMat( CvFileStorage* fs, const char* name,
                   const void* struct_ptr, CvAttrList /*attr*/ )
{
    CvMemStorage* memstorage = 0;
    const CvSparseMat* mat = (const CvSparseMat*)struct_ptr;
    CvSparseMatIterator iterator;
    CvSparseNode* node;
    CvSeq* elements;
    CvSeqReader reader;
    int i, dims;
    int *prev_idx = 0;
    char dt[16];

    assert( CV_IS_SPARSE_MAT(mat) );

    memstorage = cvCreateMemStorage();

    cvStartWriteStruct( fs, name, CV_NODE_MAP, CV_TYPE_NAME_SPARSE_MAT );
    dims = cvGetDims( mat, 0 );

    cvStartWriteStruct( fs, "sizes", CV_NODE_SEQ + CV_NODE_FLOW );
    cvWriteRawData( fs, mat->size, dims, "i" );
    cvEndWriteStruct( fs );
    cvWriteString( fs, "dt", icvEncodeFormat( CV_MAT_TYPE(mat->type), dt ), 0 );
    cvStartWriteStruct( fs, "data", CV_NODE_SEQ + CV_NODE_FLOW );

    elements = cvCreateSeq( CV_SEQ_ELTYPE_PTR, sizeof(CvSeq), sizeof(int*), memstorage );

    node = cvInitSparseMatIterator( mat, &iterator );
    while( node )
    {
        int* idx = CV_NODE_IDX( mat, node );
        cvSeqPush( elements, &idx );
        node = cvGetNextSparseNode( &iterator );
    }

    cvSeqSort( elements, icvSortIdxCmpFunc, &dims );
    cvStartReadSeq( elements, &reader, 0 );

    for( i = 0; i < elements->total; i++ )
    {
        int* idx;
        void* val;
        int k = 0;

        CV_READ_SEQ_ELEM( idx, reader );
        if( i > 0 )
        {
            for( ; idx[k] == prev_idx[k]; k++ )
                assert( k < dims );
            if( k < dims - 1 )
                fs->write_int( fs, 0, k - dims + 1 );
        }
        for( ; k < dims; k++ )
            fs->write_int( fs, 0, idx[k] );
        prev_idx = idx;

        node = (CvSparseNode*)((uchar*)idx - mat->idxoffset );
        val = CV_NODE_VAL( mat, node );

        cvWriteRawData( fs, val, 1, dt );
    }

    cvEndWriteStruct( fs );
    cvEndWriteStruct( fs );
    cvReleaseMemStorage( &memstorage );
}


static void*
icvReadSparseMat( CvFileStorage* fs, CvFileNode* node )
{
    void* ptr = 0;
    CvSparseMat* mat;
    const char* dt;
    CvFileNode* data;
    CvFileNode* sizes_node;
    CvSeqReader reader;
    CvSeq* elements;
    int sizes[CV_MAX_DIM_HEAP], dims, elem_type, cn;
    int i;

    sizes_node = cvGetFileNodeByName( fs, node, "sizes" );
    dt = cvReadStringByName( fs, node, "dt", 0 );

    if( !sizes_node || !dt )
        CV_Error( CV_StsError, "Some of essential matrix attributes are absent" );

    dims = CV_NODE_IS_SEQ(sizes_node->tag) ? sizes_node->data.seq->total :
           CV_NODE_IS_INT(sizes_node->tag) ? 1 : -1;

    if( dims <= 0 || dims > CV_MAX_DIM)
        CV_Error( CV_StsParseError, "Could not determine sparse matrix dimensionality" );

    cvReadRawData( fs, sizes_node, sizes, "i" );
    elem_type = icvDecodeSimpleFormat( dt );

    data = cvGetFileNodeByName( fs, node, "data" );
    if( !data || !CV_NODE_IS_SEQ(data->tag) )
        CV_Error( CV_StsError, "The matrix data is not found in file storage" );

    mat = cvCreateSparseMat( dims, sizes, elem_type );

    cn = CV_MAT_CN(elem_type);
    int idx[CV_MAX_DIM_HEAP];
    elements = data->data.seq;
    cvStartReadRawData( fs, data, &reader );

    for( i = 0; i < elements->total; )
    {
        CvFileNode* elem = (CvFileNode*)reader.ptr;
        uchar* val;
        int k;
        if( !CV_NODE_IS_INT(elem->tag ))
            CV_Error( CV_StsParseError, "Sparse matrix data is corrupted" );
        k = elem->data.i;
        if( i > 0 && k >= 0 )
            idx[dims-1] = k;
        else
        {
            if( i > 0 )
                k = dims + k - 1;
            else
                idx[0] = k, k = 1;
            for( ; k < dims; k++ )
            {
                CV_NEXT_SEQ_ELEM( elements->elem_size, reader );
                i++;
                elem = (CvFileNode*)reader.ptr;
                if( !CV_NODE_IS_INT(elem->tag ) || elem->data.i < 0 )
                    CV_Error( CV_StsParseError, "Sparse matrix data is corrupted" );
                idx[k] = elem->data.i;
            }
        }
        CV_NEXT_SEQ_ELEM( elements->elem_size, reader );
        i++;
        val = cvPtrND( mat, idx, 0, 1, 0 );
        cvReadRawDataSlice( fs, &reader, cn, val, dt );
        i += cn;
    }

    ptr = mat;
    return ptr;
}


/******************************* IplImage ******************************/

static int
icvIsImage( const void* ptr )
{
    return CV_IS_IMAGE_HDR(ptr);
}

static void
icvWriteImage( CvFileStorage* fs, const char* name,
               const void* struct_ptr, CvAttrList /*attr*/ )
{
    const IplImage* image = (const IplImage*)struct_ptr;
    char dt_buf[16], *dt;
    CvSize size;
    int y, depth;

    assert( CV_IS_IMAGE(image) );

    if( image->dataOrder == IPL_DATA_ORDER_PLANE )
        CV_Error( CV_StsUnsupportedFormat,
        "Images with planar data layout are not supported" );

    cvStartWriteStruct( fs, name, CV_NODE_MAP, CV_TYPE_NAME_IMAGE );
    cvWriteInt( fs, "width", image->width );
    cvWriteInt( fs, "height", image->height );
    cvWriteString( fs, "origin", image->origin == IPL_ORIGIN_TL
                   ? "top-left" : "bottom-left", 0 );
    cvWriteString( fs, "layout", image->dataOrder == IPL_DATA_ORDER_PLANE
                   ? "planar" : "interleaved", 0 );
    if( image->roi )
    {
        cvStartWriteStruct( fs, "roi", CV_NODE_MAP + CV_NODE_FLOW );
        cvWriteInt( fs, "x", image->roi->xOffset );
        cvWriteInt( fs, "y", image->roi->yOffset );
        cvWriteInt( fs, "width", image->roi->width );
        cvWriteInt( fs, "height", image->roi->height );
        cvWriteInt( fs, "coi", image->roi->coi );
        cvEndWriteStruct( fs );
    }

    depth = IPL2CV_DEPTH(image->depth);
    CV_Assert(depth < 9);
    sprintf( dt_buf, "%d%c", image->nChannels, icvTypeSymbol[depth] );
    dt = dt_buf + (dt_buf[2] == '\0' && dt_buf[0] == '1');
    cvWriteString( fs, "dt", dt, 0 );

    size = cvSize(image->width, image->height);
    if( size.width*image->nChannels*CV_ELEM_SIZE(depth) == image->widthStep )
    {
        size.width *= size.height;
        size.height = 1;
    }

    cvStartWriteStruct( fs, "data", CV_NODE_SEQ + CV_NODE_FLOW );
    for( y = 0; y < size.height; y++ )
        cvWriteRawData( fs, image->imageData + y*image->widthStep, size.width, dt );
    cvEndWriteStruct( fs );
    cvEndWriteStruct( fs );
}


static void*
icvReadImage( CvFileStorage* fs, CvFileNode* node )
{
    void* ptr = 0;
    IplImage* image;
    const char* dt;
    CvFileNode* data;
    CvFileNode* roi_node;
    CvSeqReader reader;
    CvRect roi;
    int y, width, height, elem_type, coi, depth;
    const char* origin, *data_order;

    width = cvReadIntByName( fs, node, "width", 0 );
    height = cvReadIntByName( fs, node, "height", 0 );
    dt = cvReadStringByName( fs, node, "dt", 0 );
    origin = cvReadStringByName( fs, node, "origin", 0 );

    if( width == 0 || height == 0 || dt == 0 || origin == 0 )
        CV_Error( CV_StsError, "Some of essential image attributes are absent" );

    elem_type = icvDecodeSimpleFormat( dt );
    data_order = cvReadStringByName( fs, node, "layout", "interleaved" );
    if( !data_order || strcmp( data_order, "interleaved" ) != 0 )
        CV_Error( CV_StsError, "Only interleaved images can be read" );

    data = cvGetFileNodeByName( fs, node, "data" );
    if( !data )
        CV_Error( CV_StsError, "The image data is not found in file storage" );

    if( icvFileNodeSeqLen( data ) != width*height*CV_MAT_CN(elem_type) )
        CV_Error( CV_StsUnmatchedSizes,
        "The matrix size does not match to the number of stored elements" );

    depth = cvIplDepth(elem_type);
    image = cvCreateImage( cvSize(width,height), depth, CV_MAT_CN(elem_type) );

    roi_node = cvGetFileNodeByName( fs, node, "roi" );
    if( roi_node )
    {
        roi.x = cvReadIntByName( fs, roi_node, "x", 0 );
        roi.y = cvReadIntByName( fs, roi_node, "y", 0 );
        roi.width = cvReadIntByName( fs, roi_node, "width", 0 );
        roi.height = cvReadIntByName( fs, roi_node, "height", 0 );
        coi = cvReadIntByName( fs, roi_node, "coi", 0 );

        cvSetImageROI( image, roi );
        cvSetImageCOI( image, coi );
    }

    if( width*CV_ELEM_SIZE(elem_type) == image->widthStep )
    {
        width *= height;
        height = 1;
    }

    width *= CV_MAT_CN(elem_type);
    cvStartReadRawData( fs, data, &reader );
    for( y = 0; y < height; y++ )
    {
        cvReadRawDataSlice( fs, &reader, width,
            image->imageData + y*image->widthStep, dt );
    }

    ptr = image;
    return ptr;
}


/******************************* CvSeq ******************************/

static int
icvIsSeq( const void* ptr )
{
    return CV_IS_SEQ(ptr);
}


static void
icvReleaseSeq( void** ptr )
{
    if( !ptr )
        CV_Error( CV_StsNullPtr, "NULL double pointer" );
    *ptr = 0; // it's impossible now to release seq, so just clear the pointer
}


static void*
icvCloneSeq( const void* ptr )
{
    return cvSeqSlice( (CvSeq*)ptr, CV_WHOLE_SEQ,
                       0 /* use the same storage as for the original sequence */, 1 );
}


static void
icvWriteHeaderData( CvFileStorage* fs, const CvSeq* seq,
                    CvAttrList* attr, int initial_header_size )
{
    char header_dt_buf[128];
    const char* header_dt = cvAttrValue( attr, "header_dt" );

    if( header_dt )
    {
        int dt_header_size;
        dt_header_size = icvCalcElemSize( header_dt, initial_header_size );
        if( dt_header_size > seq->header_size )
            CV_Error( CV_StsUnmatchedSizes,
            "The size of header calculated from \"header_dt\" is greater than header_size" );
    }
    else if( seq->header_size > initial_header_size )
    {
        if( CV_IS_SEQ(seq) && CV_IS_SEQ_POINT_SET(seq) &&
            seq->header_size == sizeof(CvPoint2DSeq) &&
            seq->elem_size == sizeof(int)*2 )
        {
            CvPoint2DSeq* point_seq = (CvPoint2DSeq*)seq;

            cvStartWriteStruct( fs, "rect", CV_NODE_MAP + CV_NODE_FLOW );
            cvWriteInt( fs, "x", point_seq->rect.x );
            cvWriteInt( fs, "y", point_seq->rect.y );
            cvWriteInt( fs, "width", point_seq->rect.width );
            cvWriteInt( fs, "height", point_seq->rect.height );
            cvEndWriteStruct( fs );
            cvWriteInt( fs, "color", point_seq->color );
        }
        else if( CV_IS_SEQ(seq) && CV_IS_SEQ_CHAIN(seq) &&
                 CV_MAT_TYPE(seq->flags) == CV_8UC1 )
        {
            CvChain* chain = (CvChain*)seq;

            cvStartWriteStruct( fs, "origin", CV_NODE_MAP + CV_NODE_FLOW );
            cvWriteInt( fs, "x", chain->origin.x );
            cvWriteInt( fs, "y", chain->origin.y );
            cvEndWriteStruct( fs );
        }
        else
        {
            unsigned extra_size = seq->header_size - initial_header_size;
            // a heuristic to provide nice defaults for sequences of int's & float's
            if( extra_size % sizeof(int) == 0 )
                sprintf( header_dt_buf, "%ui", (unsigned)(extra_size/sizeof(int)) );
            else
                sprintf( header_dt_buf, "%uu", extra_size );
            header_dt = header_dt_buf;
        }
    }

    if( header_dt )
    {
        cvWriteString( fs, "header_dt", header_dt, 0 );
        cvStartWriteStruct( fs, "header_user_data", CV_NODE_SEQ + CV_NODE_FLOW );
        cvWriteRawData( fs, (uchar*)seq + sizeof(CvSeq), 1, header_dt );
        cvEndWriteStruct( fs );
    }
}


static char*
icvGetFormat( const CvSeq* seq, const char* dt_key, CvAttrList* attr,
              int initial_elem_size, char* dt_buf )
{
    char* dt = 0;
    dt = (char*)cvAttrValue( attr, dt_key );

    if( dt )
    {
        int dt_elem_size;
        dt_elem_size = icvCalcElemSize( dt, initial_elem_size );
        if( dt_elem_size != seq->elem_size )
            CV_Error( CV_StsUnmatchedSizes,
            "The size of element calculated from \"dt\" and "
            "the elem_size do not match" );
    }
    else if( CV_MAT_TYPE(seq->flags) != 0 || seq->elem_size == 1 )
    {
        if( CV_ELEM_SIZE(seq->flags) != seq->elem_size )
            CV_Error( CV_StsUnmatchedSizes,
            "Size of sequence element (elem_size) is inconsistent with seq->flags" );
        dt = icvEncodeFormat( CV_MAT_TYPE(seq->flags), dt_buf );
    }
    else if( seq->elem_size > initial_elem_size )
    {
        unsigned extra_elem_size = seq->elem_size - initial_elem_size;
        // a heuristic to provide nice defaults for sequences of int's & float's
        if( extra_elem_size % sizeof(int) == 0 )
            sprintf( dt_buf, "%ui", (unsigned)(extra_elem_size/sizeof(int)) );
        else
            sprintf( dt_buf, "%uu", extra_elem_size );
        dt = dt_buf;
    }

    return dt;
}


static void
icvWriteSeq( CvFileStorage* fs, const char* name,
             const void* struct_ptr,
             CvAttrList attr, int level )
{
    const CvSeq* seq = (CvSeq*)struct_ptr;
    CvSeqBlock* block;
    char buf[128];
    char dt_buf[128], *dt;

    assert( CV_IS_SEQ( seq ));
    cvStartWriteStruct( fs, name, CV_NODE_MAP, CV_TYPE_NAME_SEQ );

    if( level >= 0 )
        cvWriteInt( fs, "level", level );

    dt = icvGetFormat( seq, "dt", &attr, 0, dt_buf );

    strcpy(buf, "");
    if( CV_IS_SEQ_CLOSED(seq) )
        strcat(buf, " closed");
    if( CV_IS_SEQ_HOLE(seq) )
        strcat(buf, " hole");
    if( CV_IS_SEQ_CURVE(seq) )
        strcat(buf, " curve");
    if( CV_SEQ_ELTYPE(seq) == 0 && seq->elem_size != 1 )
        strcat(buf, " untyped");

    cvWriteString( fs, "flags", buf + (buf[0] ? 1 : 0), 1 );

    cvWriteInt( fs, "count", seq->total );

    cvWriteString( fs, "dt", dt, 0 );

    icvWriteHeaderData( fs, seq, &attr, sizeof(CvSeq) );
    cvStartWriteStruct( fs, "data", CV_NODE_SEQ + CV_NODE_FLOW );

    for( block = seq->first; block; block = block->next )
    {
        cvWriteRawData( fs, block->data, block->count, dt );
        if( block == seq->first->prev )
            break;
    }
    cvEndWriteStruct( fs );
    cvEndWriteStruct( fs );
}


static void
icvWriteSeqTree( CvFileStorage* fs, const char* name,
                 const void* struct_ptr, CvAttrList attr )
{
    const CvSeq* seq = (CvSeq*)struct_ptr;
    const char* recursive_value = cvAttrValue( &attr, "recursive" );
    int is_recursive = recursive_value &&
                       strcmp(recursive_value,"0") != 0 &&
                       strcmp(recursive_value,"false") != 0 &&
                       strcmp(recursive_value,"False") != 0 &&
                       strcmp(recursive_value,"FALSE") != 0;

    assert( CV_IS_SEQ( seq ));

    if( !is_recursive )
    {
        icvWriteSeq( fs, name, seq, attr, -1 );
    }
    else
    {
        CvTreeNodeIterator tree_iterator;

        cvStartWriteStruct( fs, name, CV_NODE_MAP, CV_TYPE_NAME_SEQ_TREE );
        cvStartWriteStruct( fs, "sequences", CV_NODE_SEQ );
        cvInitTreeNodeIterator( &tree_iterator, seq, INT_MAX );

        for(;;)
        {
            if( !tree_iterator.node )
                break;
            icvWriteSeq( fs, 0, tree_iterator.node, attr, tree_iterator.level );
            cvNextTreeNode( &tree_iterator );
        }

        cvEndWriteStruct( fs );
        cvEndWriteStruct( fs );
    }
}


static void*
icvReadSeq( CvFileStorage* fs, CvFileNode* node )
{
    void* ptr = 0;
    CvSeq* seq;
    CvSeqBlock* block;
    CvFileNode *data, *header_node, *rect_node, *origin_node;
    CvSeqReader reader;
    int total, flags;
    int elem_size, header_size = sizeof(CvSeq);
    int fmt_pairs[CV_FS_MAX_FMT_PAIRS], i, fmt_pair_count;
    int items_per_elem = 0;
    const char* flags_str;
    const char* header_dt;
    const char* dt;
    char* endptr = 0;

    flags_str = cvReadStringByName( fs, node, "flags", 0 );
    total = cvReadIntByName( fs, node, "count", -1 );
    dt = cvReadStringByName( fs, node, "dt", 0 );

    if( !flags_str || total == -1 || !dt )
        CV_Error( CV_StsError, "Some of essential sequence attributes are absent" );

    flags = CV_SEQ_MAGIC_VAL;

    if( cv_isdigit(flags_str[0]) )
    {
        const int OLD_SEQ_ELTYPE_BITS = 9;
        const int OLD_SEQ_ELTYPE_MASK = (1 << OLD_SEQ_ELTYPE_BITS) - 1;
        const int OLD_SEQ_KIND_BITS = 3;
        const int OLD_SEQ_KIND_MASK = ((1 << OLD_SEQ_KIND_BITS) - 1) << OLD_SEQ_ELTYPE_BITS;
        const int OLD_SEQ_KIND_CURVE = 1 << OLD_SEQ_ELTYPE_BITS;
        const int OLD_SEQ_FLAG_SHIFT = OLD_SEQ_KIND_BITS + OLD_SEQ_ELTYPE_BITS;
        const int OLD_SEQ_FLAG_CLOSED = 1 << OLD_SEQ_FLAG_SHIFT;
        const int OLD_SEQ_FLAG_HOLE = 8 << OLD_SEQ_FLAG_SHIFT;

        int flags0 = (int)strtol( flags_str, &endptr, 16 );
        if( endptr == flags_str || (flags0 & CV_MAGIC_MASK) != CV_SEQ_MAGIC_VAL )
            CV_Error( CV_StsError, "The sequence flags are invalid" );
        if( (flags0 & OLD_SEQ_KIND_MASK) == OLD_SEQ_KIND_CURVE )
            flags |= CV_SEQ_KIND_CURVE;
        if( flags0 & OLD_SEQ_FLAG_CLOSED )
            flags |= CV_SEQ_FLAG_CLOSED;
        if( flags0 & OLD_SEQ_FLAG_HOLE )
            flags |= CV_SEQ_FLAG_HOLE;
        flags |= flags0 & OLD_SEQ_ELTYPE_MASK;
    }
    else
    {
        if( strstr(flags_str, "curve") )
            flags |= CV_SEQ_KIND_CURVE;
        if( strstr(flags_str, "closed") )
            flags |= CV_SEQ_FLAG_CLOSED;
        if( strstr(flags_str, "hole") )
            flags |= CV_SEQ_FLAG_HOLE;
        if( !strstr(flags_str, "untyped") )
        {
            CV_TRY
            {
                flags |= icvDecodeSimpleFormat(dt);
            }
            CV_CATCH_ALL
            {
            }
        }
    }

    header_dt = cvReadStringByName( fs, node, "header_dt", 0 );
    header_node = cvGetFileNodeByName( fs, node, "header_user_data" );

    if( (header_dt != 0) ^ (header_node != 0) )
        CV_Error( CV_StsError,
        "One of \"header_dt\" and \"header_user_data\" is there, while the other is not" );

    rect_node = cvGetFileNodeByName( fs, node, "rect" );
    origin_node = cvGetFileNodeByName( fs, node, "origin" );

    if( (header_node != 0) + (rect_node != 0) + (origin_node != 0) > 1 )
        CV_Error( CV_StsError, "Only one of \"header_user_data\", \"rect\" and \"origin\" tags may occur" );

    if( header_dt )
    {
        header_size = icvCalcElemSize( header_dt, header_size );
    }
    else if( rect_node )
        header_size = sizeof(CvPoint2DSeq);
    else if( origin_node )
        header_size = sizeof(CvChain);

    elem_size = icvCalcElemSize( dt, 0 );
    seq = cvCreateSeq( flags, header_size, elem_size, fs->dststorage );

    if( header_node )
    {
        CV_Assert(header_dt);
        cvReadRawData( fs, header_node, (char*)seq + sizeof(CvSeq), header_dt );
    }
    else if( rect_node )
    {
        CvPoint2DSeq* point_seq = (CvPoint2DSeq*)seq;
        point_seq->rect.x = cvReadIntByName( fs, rect_node, "x", 0 );
        point_seq->rect.y = cvReadIntByName( fs, rect_node, "y", 0 );
        point_seq->rect.width = cvReadIntByName( fs, rect_node, "width", 0 );
        point_seq->rect.height = cvReadIntByName( fs, rect_node, "height", 0 );
        point_seq->color = cvReadIntByName( fs, node, "color", 0 );
    }
    else if( origin_node )
    {
        CvChain* chain = (CvChain*)seq;
        chain->origin.x = cvReadIntByName( fs, origin_node, "x", 0 );
        chain->origin.y = cvReadIntByName( fs, origin_node, "y", 0 );
    }

    cvSeqPushMulti( seq, 0, total, 0 );
    fmt_pair_count = icvDecodeFormat( dt, fmt_pairs, CV_FS_MAX_FMT_PAIRS );
    fmt_pair_count *= 2;
    for( i = 0; i < fmt_pair_count; i += 2 )
        items_per_elem += fmt_pairs[i];

    data = cvGetFileNodeByName( fs, node, "data" );
    if( !data )
        CV_Error( CV_StsError, "The image data is not found in file storage" );

    if( icvFileNodeSeqLen( data ) != total*items_per_elem )
        CV_Error( CV_StsError, "The number of stored elements does not match to \"count\"" );

    cvStartReadRawData( fs, data, &reader );
    for( block = seq->first; block; block = block->next )
    {
        int delta = block->count*items_per_elem;
        cvReadRawDataSlice( fs, &reader, delta, block->data, dt );
        if( block == seq->first->prev )
            break;
    }

    ptr = seq;
    return ptr;
}


static void*
icvReadSeqTree( CvFileStorage* fs, CvFileNode* node )
{
    void* ptr = 0;
    CvFileNode *sequences_node = cvGetFileNodeByName( fs, node, "sequences" );
    CvSeq* sequences;
    CvSeq* root = 0;
    CvSeq* parent = 0;
    CvSeq* prev_seq = 0;
    CvSeqReader reader;
    int i, total;
    int prev_level = 0;

    if( !sequences_node || !CV_NODE_IS_SEQ(sequences_node->tag) )
        CV_Error( CV_StsParseError,
        "opencv-sequence-tree instance should contain a field \"sequences\" that should be a sequence" );

    sequences = sequences_node->data.seq;
    total = sequences->total;

    cvStartReadSeq( sequences, &reader, 0 );
    for( i = 0; i < total; i++ )
    {
        CvFileNode* elem = (CvFileNode*)reader.ptr;
        CvSeq* seq;
        int level;
        seq = (CvSeq*)cvRead( fs, elem );
        CV_Assert(seq);
        level = cvReadIntByName( fs, elem, "level", -1 );
        if( level < 0 )
            CV_Error( CV_StsParseError, "All the sequence tree nodes should contain \"level\" field" );
        if( !root )
            root = seq;
        if( level > prev_level )
        {
            assert( level == prev_level + 1 );
            parent = prev_seq;
            prev_seq = 0;
            if( parent )
                parent->v_next = seq;
        }
        else if( level < prev_level )
        {
            for( ; prev_level > level; prev_level-- )
                prev_seq = prev_seq->v_prev;
            parent = prev_seq->v_prev;
        }
        seq->h_prev = prev_seq;
        if( prev_seq )
            prev_seq->h_next = seq;
        seq->v_prev = parent;
        prev_seq = seq;
        prev_level = level;
        CV_NEXT_SEQ_ELEM( sequences->elem_size, reader );
    }

    ptr = root;
    return ptr;
}

/******************************* CvGraph ******************************/

static int
icvIsGraph( const void* ptr )
{
    return CV_IS_GRAPH(ptr);
}


static void
icvReleaseGraph( void** ptr )
{
    if( !ptr )
        CV_Error( CV_StsNullPtr, "NULL double pointer" );

    *ptr = 0; // it's impossible now to release graph, so just clear the pointer
}


static void*
icvCloneGraph( const void* ptr )
{
    return cvCloneGraph( (const CvGraph*)ptr, 0 );
}


static void
icvWriteGraph( CvFileStorage* fs, const char* name,
               const void* struct_ptr, CvAttrList attr )
{
    int* flag_buf = 0;
    char* write_buf = 0;
    const CvGraph* graph = (const CvGraph*)struct_ptr;
    CvSeqReader reader;
    char buf[128];
    int i, k, vtx_count, edge_count;
    char vtx_dt_buf[128], *vtx_dt;
    char edge_dt_buf[128], *edge_dt;
    int write_buf_size;

    assert( CV_IS_GRAPH(graph) );
    vtx_count = cvGraphGetVtxCount( graph );
    edge_count = cvGraphGetEdgeCount( graph );
    flag_buf = (int*)cvAlloc( vtx_count*sizeof(flag_buf[0]));

    // count vertices
    cvStartReadSeq( (CvSeq*)graph, &reader );
    for( i = 0, k = 0; i < graph->total; i++ )
    {
        if( CV_IS_SET_ELEM( reader.ptr ))
        {
            CvGraphVtx* vtx = (CvGraphVtx*)reader.ptr;
            flag_buf[k] = vtx->flags;
            vtx->flags = k++;
        }
        CV_NEXT_SEQ_ELEM( graph->elem_size, reader );
    }

    // write header
    cvStartWriteStruct( fs, name, CV_NODE_MAP, CV_TYPE_NAME_GRAPH );

    cvWriteString(fs, "flags", CV_IS_GRAPH_ORIENTED(graph) ? "oriented" : "", 1);

    cvWriteInt( fs, "vertex_count", vtx_count );
    vtx_dt = icvGetFormat( (CvSeq*)graph, "vertex_dt",
                    &attr, sizeof(CvGraphVtx), vtx_dt_buf );
    if( vtx_dt )
        cvWriteString( fs, "vertex_dt", vtx_dt, 0 );

    cvWriteInt( fs, "edge_count", edge_count );
    edge_dt = icvGetFormat( (CvSeq*)graph->edges, "edge_dt",
                                &attr, sizeof(CvGraphEdge), buf );
    sprintf( edge_dt_buf, "2if%s", edge_dt ? edge_dt : "" );
    edge_dt = edge_dt_buf;
    cvWriteString( fs, "edge_dt", edge_dt, 0 );

    icvWriteHeaderData( fs, (CvSeq*)graph, &attr, sizeof(CvGraph) );

    write_buf_size = MAX( 3*graph->elem_size, 1 << 16 );
    write_buf_size = MAX( 3*graph->edges->elem_size, write_buf_size );
    write_buf = (char*)cvAlloc( write_buf_size );

    // as vertices and edges are written in similar way,
    // do it as a parametrized 2-iteration loop
    for( k = 0; k < 2; k++ )
    {
        const char* dt = k == 0 ? vtx_dt : edge_dt;
        if( dt )
        {
            CvSet* data = k == 0 ? (CvSet*)graph : graph->edges;
            int elem_size = data->elem_size;
            int write_elem_size = icvCalcElemSize( dt, 0 );
            char* src_ptr = write_buf;
            int write_max = write_buf_size / write_elem_size, write_count = 0;

            // alignment of user part of the edge data following 2if
            int edge_user_align = sizeof(float);

            if( k == 1 )
            {
                int fmt_pairs[CV_FS_MAX_FMT_PAIRS], fmt_pair_count;
                fmt_pair_count = icvDecodeFormat( dt, fmt_pairs, CV_FS_MAX_FMT_PAIRS );
                if( fmt_pair_count > 2 && CV_ELEM_SIZE(fmt_pairs[2*2+1]) >= (int)sizeof(double))
                    edge_user_align = sizeof(double);
            }

            cvStartWriteStruct( fs, k == 0 ? "vertices" : "edges",
                                CV_NODE_SEQ + CV_NODE_FLOW );
            cvStartReadSeq( (CvSeq*)data, &reader );
            for( i = 0; i < data->total; i++ )
            {
                if( CV_IS_SET_ELEM( reader.ptr ))
                {
                    if( k == 0 ) // vertices
                        memcpy( src_ptr, reader.ptr + sizeof(CvGraphVtx), write_elem_size );
                    else
                    {
                        CvGraphEdge* edge = (CvGraphEdge*)reader.ptr;
                        src_ptr = (char*)cvAlignPtr( src_ptr, sizeof(int) );
                        ((int*)src_ptr)[0] = edge->vtx[0]->flags;
                        ((int*)src_ptr)[1] = edge->vtx[1]->flags;
                        *(float*)(src_ptr + sizeof(int)*2) = edge->weight;
                        if( elem_size > (int)sizeof(CvGraphEdge) )
                        {
                            char* src_ptr2 = (char*)cvAlignPtr( src_ptr + 2*sizeof(int)
                                                + sizeof(float), edge_user_align );
                            memcpy( src_ptr2, edge + 1, elem_size - sizeof(CvGraphEdge) );
                        }
                    }
                    src_ptr += write_elem_size;
                    if( ++write_count >= write_max )
                    {
                        cvWriteRawData( fs, write_buf, write_count, dt );
                        write_count = 0;
                        src_ptr = write_buf;
                    }
                }
                CV_NEXT_SEQ_ELEM( data->elem_size, reader );
            }

            if( write_count > 0 )
                cvWriteRawData( fs, write_buf, write_count, dt );
            cvEndWriteStruct( fs );
        }
    }

    cvEndWriteStruct( fs );

    // final stage. restore the graph flags
    cvStartReadSeq( (CvSeq*)graph, &reader );
    vtx_count = 0;
    for( i = 0; i < graph->total; i++ )
    {
        if( CV_IS_SET_ELEM( reader.ptr ))
            ((CvGraphVtx*)reader.ptr)->flags = flag_buf[vtx_count++];
        CV_NEXT_SEQ_ELEM( graph->elem_size, reader );
    }

    cvFree( &write_buf );
    cvFree( &flag_buf );
}


static void*
icvReadGraph( CvFileStorage* fs, CvFileNode* node )
{
    void* ptr = 0;
    char* read_buf = 0;
    CvGraphVtx** vtx_buf = 0;
    CvGraph* graph;
    CvFileNode *header_node, *vtx_node, *edge_node;
    int flags, vtx_count, edge_count;
    int vtx_size = sizeof(CvGraphVtx), edge_size, header_size = sizeof(CvGraph);
    int src_vtx_size = 0, src_edge_size;
    int fmt_pairs[CV_FS_MAX_FMT_PAIRS], fmt_pair_count;
    int vtx_items_per_elem = 0, edge_items_per_elem = 0;
    int edge_user_align = sizeof(float);
    int read_buf_size;
    int i, k;
    const char* flags_str;
    const char* header_dt;
    const char* vtx_dt;
    const char* edge_dt;
    char* endptr = 0;

    flags_str = cvReadStringByName( fs, node, "flags", 0 );
    vtx_dt = cvReadStringByName( fs, node, "vertex_dt", 0 );
    edge_dt = cvReadStringByName( fs, node, "edge_dt", 0 );
    vtx_count = cvReadIntByName( fs, node, "vertex_count", -1 );
    edge_count = cvReadIntByName( fs, node, "edge_count", -1 );

    if( !flags_str || vtx_count == -1 || edge_count == -1 || !edge_dt )
        CV_Error( CV_StsError, "Some of essential graph attributes are absent" );

    flags = CV_SET_MAGIC_VAL + CV_GRAPH;

    if( isxdigit(flags_str[0]) )
    {
        const int OLD_SEQ_ELTYPE_BITS = 9;
        const int OLD_SEQ_KIND_BITS = 3;
        const int OLD_SEQ_FLAG_SHIFT = OLD_SEQ_KIND_BITS + OLD_SEQ_ELTYPE_BITS;
        const int OLD_GRAPH_FLAG_ORIENTED = 1 << OLD_SEQ_FLAG_SHIFT;

        int flags0 = (int)strtol( flags_str, &endptr, 16 );
        if( endptr == flags_str || (flags0 & CV_MAGIC_MASK) != CV_SET_MAGIC_VAL )
            CV_Error( CV_StsError, "The sequence flags are invalid" );
        if( flags0 & OLD_GRAPH_FLAG_ORIENTED )
            flags |= CV_GRAPH_FLAG_ORIENTED;
    }
    else
    {
        if( strstr(flags_str, "oriented") )
            flags |= CV_GRAPH_FLAG_ORIENTED;
    }

    header_dt = cvReadStringByName( fs, node, "header_dt", 0 );
    header_node = cvGetFileNodeByName( fs, node, "header_user_data" );

    if( (header_dt != 0) ^ (header_node != 0) )
        CV_Error( CV_StsError,
        "One of \"header_dt\" and \"header_user_data\" is there, while the other is not" );

    if( header_dt )
        header_size = icvCalcElemSize( header_dt, header_size );

    if( vtx_dt )
    {
        src_vtx_size = icvCalcElemSize( vtx_dt, 0 );
        vtx_size = icvCalcElemSize( vtx_dt, vtx_size );
        fmt_pair_count = icvDecodeFormat( edge_dt,
                            fmt_pairs, CV_FS_MAX_FMT_PAIRS );
        fmt_pair_count *= 2;
        for( i = 0; i < fmt_pair_count; i += 2 )
            vtx_items_per_elem += fmt_pairs[i];
    }

    {
        char dst_edge_dt_buf[128];
        const char* dst_edge_dt = 0;

        fmt_pair_count = icvDecodeFormat( edge_dt,
                            fmt_pairs, CV_FS_MAX_FMT_PAIRS );
        if( fmt_pair_count < 2 ||
            fmt_pairs[0] != 2 || fmt_pairs[1] != CV_32S ||
            fmt_pairs[2] < 1 || fmt_pairs[3] != CV_32F )
            CV_Error( CV_StsBadArg,
            "Graph edges should start with 2 integers and a float" );

        // alignment of user part of the edge data following 2if
        if( fmt_pair_count > 2 && CV_ELEM_SIZE(fmt_pairs[5]) >= (int)sizeof(double))
            edge_user_align = sizeof(double);

        fmt_pair_count *= 2;
        for( i = 0; i < fmt_pair_count; i += 2 )
            edge_items_per_elem += fmt_pairs[i];

        if( edge_dt[2] == 'f' || (edge_dt[2] == '1' && edge_dt[3] == 'f') )
            dst_edge_dt = edge_dt + 3 + cv_isdigit(edge_dt[2]);
        else
        {
            int val = (int)strtol( edge_dt + 2, &endptr, 10 );
            sprintf( dst_edge_dt_buf, "%df%s", val-1, endptr );
            dst_edge_dt = dst_edge_dt_buf;
        }

        edge_size = icvCalcElemSize( dst_edge_dt, sizeof(CvGraphEdge) );
        src_edge_size = icvCalcElemSize( edge_dt, 0 );
    }

    graph = cvCreateGraph( flags, header_size, vtx_size, edge_size, fs->dststorage );

    if( header_node )
    {
        CV_Assert(header_dt);
        cvReadRawData( fs, header_node, (char*)graph + sizeof(CvGraph), header_dt );
    }

    read_buf_size = MAX( src_vtx_size*3, 1 << 16 );
    read_buf_size = MAX( src_edge_size*3, read_buf_size );
    read_buf = (char*)cvAlloc( read_buf_size );
    vtx_buf = (CvGraphVtx**)cvAlloc( vtx_count * sizeof(vtx_buf[0]) );

    vtx_node = cvGetFileNodeByName( fs, node, "vertices" );
    edge_node = cvGetFileNodeByName( fs, node, "edges" );
    if( !edge_node )
        CV_Error( CV_StsBadArg, "No edges data" );
    if( vtx_dt && !vtx_node )
        CV_Error( CV_StsBadArg, "No vertices data" );

    // as vertices and edges are read in similar way,
    // do it as a parametrized 2-iteration loop
    for( k = 0; k < 2; k++ )
    {
        const char* dt = k == 0 ? vtx_dt : edge_dt;
        int elem_size = k == 0 ? vtx_size : edge_size;
        int src_elem_size = k == 0 ? src_vtx_size : src_edge_size;
        int items_per_elem = k == 0 ? vtx_items_per_elem : edge_items_per_elem;
        int elem_count = k == 0 ? vtx_count : edge_count;
        char* dst_ptr = read_buf;
        int read_max = read_buf_size /MAX(src_elem_size, 1), read_count = 0;
        CvSeqReader reader;
        if(dt)
            cvStartReadRawData( fs, k == 0 ? vtx_node : edge_node, &reader );

        for( i = 0; i < elem_count; i++ )
        {
            if( read_count == 0 && dt )
            {
                int count = MIN( elem_count - i, read_max )*items_per_elem;
                cvReadRawDataSlice( fs, &reader, count, read_buf, dt );
                read_count = count;
                dst_ptr = read_buf;
            }

            if( k == 0 )
            {
                CvGraphVtx* vtx;
                cvGraphAddVtx( graph, 0, &vtx );
                vtx_buf[i] = vtx;
                if( dt )
                    memcpy( vtx + 1, dst_ptr, src_elem_size );
            }
            else
            {
                CvGraphEdge* edge = 0;
                int vtx1 = ((int*)dst_ptr)[0];
                int vtx2 = ((int*)dst_ptr)[1];
                int result;

                if( (unsigned)vtx1 >= (unsigned)vtx_count ||
                    (unsigned)vtx2 >= (unsigned)vtx_count )
                    CV_Error( CV_StsOutOfRange,
                    "Some of stored vertex indices are out of range" );

                result = cvGraphAddEdgeByPtr( graph,
                    vtx_buf[vtx1], vtx_buf[vtx2], 0, &edge );

                if( result == 0 )
                    CV_Error( CV_StsBadArg, "Duplicated edge has occured" );

                edge->weight = *(float*)(dst_ptr + sizeof(int)*2);
                if( elem_size > (int)sizeof(CvGraphEdge) )
                {
                    char* dst_ptr2 = (char*)cvAlignPtr( dst_ptr + sizeof(int)*2 +
                                                sizeof(float), edge_user_align );
                    memcpy( edge + 1, dst_ptr2, elem_size - sizeof(CvGraphEdge) );
                }
            }

            dst_ptr += src_elem_size;
            read_count--;
        }
    }

    ptr = graph;
    cvFree( &read_buf );
    cvFree( &vtx_buf );

    return ptr;
}

/****************************************************************************************\
*                                    RTTI Functions                                      *
\****************************************************************************************/

CvTypeInfo *CvType::first = 0, *CvType::last = 0;

CvType::CvType( const char* type_name,
                CvIsInstanceFunc is_instance, CvReleaseFunc release,
                CvReadFunc read, CvWriteFunc write, CvCloneFunc clone )
{
    CvTypeInfo _info;
    _info.flags = 0;
    _info.header_size = sizeof(_info);
    _info.type_name = type_name;
    _info.prev = _info.next = 0;
    _info.is_instance = is_instance;
    _info.release = release;
    _info.clone = clone;
    _info.read = read;
    _info.write = write;

    cvRegisterType( &_info );
    info = first;
}


CvType::~CvType()
{
    cvUnregisterType( info->type_name );
}


CvType seq_type( CV_TYPE_NAME_SEQ, icvIsSeq, icvReleaseSeq, icvReadSeq,
                 icvWriteSeqTree /* this is the entry point for
                 writing a single sequence too */, icvCloneSeq );

CvType seq_tree_type( CV_TYPE_NAME_SEQ_TREE, icvIsSeq, icvReleaseSeq,
                      icvReadSeqTree, icvWriteSeqTree, icvCloneSeq );

CvType seq_graph_type( CV_TYPE_NAME_GRAPH, icvIsGraph, icvReleaseGraph,
                       icvReadGraph, icvWriteGraph, icvCloneGraph );

CvType sparse_mat_type( CV_TYPE_NAME_SPARSE_MAT, icvIsSparseMat,
                        (CvReleaseFunc)cvReleaseSparseMat, icvReadSparseMat,
                        icvWriteSparseMat, (CvCloneFunc)cvCloneSparseMat );

CvType image_type( CV_TYPE_NAME_IMAGE, icvIsImage, (CvReleaseFunc)cvReleaseImage,
                   icvReadImage, icvWriteImage, (CvCloneFunc)cvCloneImage );

CvType mat_type( CV_TYPE_NAME_MAT, icvIsMat, (CvReleaseFunc)cvReleaseMat,
                 icvReadMat, icvWriteMat, (CvCloneFunc)cvCloneMat );

CvType matnd_type( CV_TYPE_NAME_MATND, icvIsMatND, (CvReleaseFunc)cvReleaseMatND,
                   icvReadMatND, icvWriteMatND, (CvCloneFunc)cvCloneMatND );

CV_IMPL  void
cvRegisterType( const CvTypeInfo* _info )
{
    CvTypeInfo* info = 0;
    int i, len;
    char c;

    //if( !CvType::first )
    //    icvCreateStandardTypes();

    if( !_info || _info->header_size != sizeof(CvTypeInfo) )
        CV_Error( CV_StsBadSize, "Invalid type info" );

    if( !_info->is_instance || !_info->release ||
        !_info->read || !_info->write )
        CV_Error( CV_StsNullPtr,
        "Some of required function pointers "
        "(is_instance, release, read or write) are NULL");

    c = _info->type_name[0];
    if( !cv_isalpha(c) && c != '_' )
        CV_Error( CV_StsBadArg, "Type name should start with a letter or _" );

    len = (int)strlen(_info->type_name);

    for( i = 0; i < len; i++ )
    {
        c = _info->type_name[i];
        if( !cv_isalnum(c) && c != '-' && c != '_' )
            CV_Error( CV_StsBadArg,
            "Type name should contain only letters, digits, - and _" );
    }

    info = (CvTypeInfo*)cvAlloc( sizeof(*info) + len + 1 );

    *info = *_info;
    info->type_name = (char*)(info + 1);
    memcpy( (char*)info->type_name, _info->type_name, len + 1 );

    info->flags = 0;
    info->next = CvType::first;
    info->prev = 0;
    if( CvType::first )
        CvType::first->prev = info;
    else
        CvType::last = info;
    CvType::first = info;
}


CV_IMPL void
cvUnregisterType( const char* type_name )
{
    CvTypeInfo* info;

    info = cvFindType( type_name );
    if( info )
    {
        if( info->prev )
            info->prev->next = info->next;
        else
            CvType::first = info->next;

        if( info->next )
            info->next->prev = info->prev;
        else
            CvType::last = info->prev;

        if( !CvType::first || !CvType::last )
            CvType::first = CvType::last = 0;

        cvFree( &info );
    }
}


CV_IMPL CvTypeInfo*
cvFirstType( void )
{
    return CvType::first;
}


CV_IMPL CvTypeInfo*
cvFindType( const char* type_name )
{
    CvTypeInfo* info = 0;

    if (type_name)
      for( info = CvType::first; info != 0; info = info->next )
        if( strcmp( info->type_name, type_name ) == 0 )
      break;

    return info;
}


CV_IMPL CvTypeInfo*
cvTypeOf( const void* struct_ptr )
{
    CvTypeInfo* info = 0;

    if( struct_ptr )
    {
        for( info = CvType::first; info != 0; info = info->next )
            if( info->is_instance( struct_ptr ))
                break;
    }

    return info;
}


/* universal functions */
CV_IMPL void
cvRelease( void** struct_ptr )
{
    CvTypeInfo* info;

    if( !struct_ptr )
        CV_Error( CV_StsNullPtr, "NULL double pointer" );

    if( *struct_ptr )
    {
        info = cvTypeOf( *struct_ptr );
        if( !info )
            CV_Error( CV_StsError, "Unknown object type" );
        if( !info->release )
            CV_Error( CV_StsError, "release function pointer is NULL" );

        info->release( struct_ptr );
        *struct_ptr = 0;
    }
}


void* cvClone( const void* struct_ptr )
{
    void* struct_copy = 0;
    CvTypeInfo* info;

    if( !struct_ptr )
        CV_Error( CV_StsNullPtr, "NULL structure pointer" );

    info = cvTypeOf( struct_ptr );
    if( !info )
        CV_Error( CV_StsError, "Unknown object type" );
    if( !info->clone )
        CV_Error( CV_StsError, "clone function pointer is NULL" );

    struct_copy = info->clone( struct_ptr );
    return struct_copy;
}


/* reads matrix, image, sequence, graph etc. */
CV_IMPL void*
cvRead( CvFileStorage* fs, CvFileNode* node, CvAttrList* list )
{
    void* obj = 0;
    CV_CHECK_FILE_STORAGE( fs );

    if( !node )
        return 0;

    if( !CV_NODE_IS_USER(node->tag) || !node->info )
        CV_Error( CV_StsError, "The node does not represent a user object (unknown type?)" );

    obj = node->info->read( fs, node );
    if( list )
        *list = cvAttrList(0,0);

    return obj;
}


/* writes matrix, image, sequence, graph etc. */
CV_IMPL void
cvWrite( CvFileStorage* fs, const char* name,
         const void* ptr, CvAttrList attributes )
{
    CvTypeInfo* info;

    CV_CHECK_OUTPUT_FILE_STORAGE( fs );

    if( !ptr )
        CV_Error( CV_StsNullPtr, "Null pointer to the written object" );

    info = cvTypeOf( ptr );
    if( !info )
        CV_Error( CV_StsBadArg, "Unknown object" );

    if( !info->write )
        CV_Error( CV_StsBadArg, "The object does not have write function" );

    info->write( fs, name, ptr, attributes );
}


/* simple API for reading/writing data */
CV_IMPL void
cvSave( const char* filename, const void* struct_ptr,
        const char* _name, const char* comment, CvAttrList attributes )
{
    CvFileStorage* fs = 0;

    if( !struct_ptr )
        CV_Error( CV_StsNullPtr, "NULL object pointer" );

    fs = cvOpenFileStorage( filename, 0, CV_STORAGE_WRITE );
    if( !fs )
        CV_Error( CV_StsError, "Could not open the file storage. Check the path and permissions" );

    cv::String name = _name ? cv::String(_name) : cv::FileStorage::getDefaultObjectName(filename);

    if( comment )
        cvWriteComment( fs, comment, 0 );
    cvWrite( fs, name.c_str(), struct_ptr, attributes );
    cvReleaseFileStorage( &fs );
}

CV_IMPL void*
cvLoad( const char* filename, CvMemStorage* memstorage,
        const char* name, const char** _real_name )
{
    void* ptr = 0;
    const char* real_name = 0;
    cv::FileStorage fs(cvOpenFileStorage(filename, memstorage, CV_STORAGE_READ));

    CvFileNode* node = 0;

    if( !fs.isOpened() )
        return 0;

    if( name )
    {
        node = cvGetFileNodeByName( *fs, 0, name );
    }
    else
    {
        int i, k;
        for( k = 0; k < (*fs)->roots->total; k++ )
        {
            CvSeq* seq;
            CvSeqReader reader;

            node = (CvFileNode*)cvGetSeqElem( (*fs)->roots, k );
            CV_Assert(node != NULL);
            if( !CV_NODE_IS_MAP( node->tag ))
                return 0;
            seq = node->data.seq;
            node = 0;

            cvStartReadSeq( seq, &reader, 0 );

            // find the first element in the map
            for( i = 0; i < seq->total; i++ )
            {
                if( CV_IS_SET_ELEM( reader.ptr ))
                {
                    node = (CvFileNode*)reader.ptr;
                    goto stop_search;
                }
                CV_NEXT_SEQ_ELEM( seq->elem_size, reader );
            }
        }

stop_search:
        ;
    }

    if( !node )
        CV_Error( CV_StsObjectNotFound, "Could not find the/an object in file storage" );

    real_name = cvGetFileNodeName( node );
    ptr = cvRead( *fs, node, 0 );

    // sanity check
    if( !memstorage && (CV_IS_SEQ( ptr ) || CV_IS_SET( ptr )) )
        CV_Error( CV_StsNullPtr,
        "NULL memory storage is passed - the loaded dynamic structure can not be stored" );

    if( cvGetErrStatus() < 0 )
    {
        cvRelease( (void**)&ptr );
        real_name = 0;
    }

    if( _real_name)
    {
    if (real_name)
    {
        *_real_name = (const char*)cvAlloc(strlen(real_name));
            memcpy((void*)*_real_name, real_name, strlen(real_name));
    } else {
        *_real_name = 0;
    }
    }

    return ptr;
}


///////////////////////// new C++ interface for CvFileStorage ///////////////////////////

namespace cv
{

static void getElemSize( const String& fmt, size_t& elemSize, size_t& cn )
{
    const char* dt = fmt.c_str();
    cn = 1;
    if( cv_isdigit(dt[0]) )
    {
        cn = dt[0] - '0';
        dt++;
    }
    char c = dt[0];
    elemSize = cn*(c == 'u' || c == 'c' ? sizeof(uchar) : c == 'w' || c == 's' ? sizeof(ushort) :
        c == 'i' ? sizeof(int) : c == 'f' ? sizeof(float) : c == 'd' ? sizeof(double) :
        c == 'r' ? sizeof(void*) : (size_t)0);
}

FileStorage::FileStorage()
{
    state = UNDEFINED;
}

FileStorage::FileStorage(const String& filename, int flags, const String& encoding)
{
    state = UNDEFINED;
    open( filename, flags, encoding );
}

FileStorage::FileStorage(CvFileStorage* _fs, bool owning)
{
    if (owning) fs.reset(_fs);
    else fs = Ptr<CvFileStorage>(Ptr<CvFileStorage>(), _fs);

    state = _fs ? NAME_EXPECTED + INSIDE_MAP : UNDEFINED;
}

FileStorage::~FileStorage()
{
    while( structs.size() > 0 )
    {
        cvEndWriteStruct(fs);
        structs.pop_back();
    }
}

bool FileStorage::open(const String& filename, int flags, const String& encoding)
{
    CV_INSTRUMENT_REGION()

    release();
    fs.reset(cvOpenFileStorage( filename.c_str(), 0, flags,
                                !encoding.empty() ? encoding.c_str() : 0));
    bool ok = isOpened();
    state = ok ? NAME_EXPECTED + INSIDE_MAP : UNDEFINED;
    return ok;
}

bool FileStorage::isOpened() const
{
    return fs && fs->is_opened;
}

void FileStorage::release()
{
    fs.release();
    structs.clear();
    state = UNDEFINED;
}

String FileStorage::releaseAndGetString()
{
    String buf;
    if( fs && fs->outbuf )
        icvClose(fs, &buf);

    release();
    return buf;
}

FileNode FileStorage::root(int streamidx) const
{
    return isOpened() ? FileNode(fs, cvGetRootFileNode(fs, streamidx)) : FileNode();
}

int FileStorage::getFormat() const
{
    CV_Assert(!fs.empty());
    return fs->fmt & FORMAT_MASK;
}

FileStorage& operator << (FileStorage& fs, const String& str)
{
    CV_TRACE_REGION_VERBOSE();

    enum { NAME_EXPECTED = FileStorage::NAME_EXPECTED,
        VALUE_EXPECTED = FileStorage::VALUE_EXPECTED,
        INSIDE_MAP = FileStorage::INSIDE_MAP };
    const char* _str = str.c_str();
    if( !fs.isOpened() || !_str )
        return fs;
    if( *_str == '}' || *_str == ']' )
    {
        if( fs.structs.empty() )
            CV_Error_( CV_StsError, ("Extra closing '%c'", *_str) );
        if( (*_str == ']' ? '[' : '{') != fs.structs.back() )
            CV_Error_( CV_StsError,
            ("The closing '%c' does not match the opening '%c'", *_str, fs.structs.back()));
        fs.structs.pop_back();
        fs.state = fs.structs.empty() || fs.structs.back() == '{' ?
            INSIDE_MAP + NAME_EXPECTED : VALUE_EXPECTED;
        cvEndWriteStruct( *fs );
        fs.elname = String();
    }
    else if( fs.state == NAME_EXPECTED + INSIDE_MAP )
    {
        if (!cv_isalpha(*_str) && *_str != '_')
            CV_Error_( CV_StsError, ("Incorrect element name %s", _str) );
        fs.elname = str;
        fs.state = VALUE_EXPECTED + INSIDE_MAP;
    }
    else if( (fs.state & 3) == VALUE_EXPECTED )
    {
        if( *_str == '{' || *_str == '[' )
        {
            fs.structs.push_back(*_str);
            int flags = *_str++ == '{' ? CV_NODE_MAP : CV_NODE_SEQ;
            fs.state = flags == CV_NODE_MAP ? INSIDE_MAP +
                NAME_EXPECTED : VALUE_EXPECTED;
            if( *_str == ':' )
            {
                flags |= CV_NODE_FLOW;
                _str++;
            }
            cvStartWriteStruct( *fs, fs.elname.size() > 0 ? fs.elname.c_str() : 0,
                flags, *_str ? _str : 0 );
            fs.elname = String();
        }
        else
        {
            write( fs, fs.elname, (_str[0] == '\\' && (_str[1] == '{' || _str[1] == '}' ||
                _str[1] == '[' || _str[1] == ']')) ? String(_str+1) : str );
            if( fs.state == INSIDE_MAP + VALUE_EXPECTED )
                fs.state = INSIDE_MAP + NAME_EXPECTED;
        }
    }
    else
        CV_Error( CV_StsError, "Invalid fs.state" );
    return fs;
}


void FileStorage::writeRaw( const String& fmt, const uchar* vec, size_t len )
{
    if( !isOpened() )
        return;
    size_t elemSize, cn;
    getElemSize( fmt, elemSize, cn );
    CV_Assert( len % elemSize == 0 );
    cvWriteRawData( fs, vec, (int)(len/elemSize), fmt.c_str());
}


void FileStorage::writeObj( const String& name, const void* obj )
{
    if( !isOpened() )
        return;
    cvWrite( fs, name.size() > 0 ? name.c_str() : 0, obj );
}

void FileStorage::write( const String& name, double val )
{
    *this << name << val;
}

void FileStorage::write( const String& name, const String& val )
{
    *this << name << val;
}

void FileStorage::write( const String& name, InputArray val )
{
    *this << name << val.getMat();
}

void FileStorage::writeComment( const String& comment, bool append )
{
    cvWriteComment(fs, comment.c_str(), append ? 1 : 0);
}

FileNode FileStorage::operator[](const String& nodename) const
{
    return FileNode(fs, cvGetFileNodeByName(fs, 0, nodename.c_str()));
}

FileNode FileStorage::operator[](const char* nodename) const
{
    return FileNode(fs, cvGetFileNodeByName(fs, 0, nodename));
}

FileNode FileNode::operator[](const String& nodename) const
{
    return FileNode(fs, cvGetFileNodeByName(fs, node, nodename.c_str()));
}

FileNode FileNode::operator[](const char* nodename) const
{
    return FileNode(fs, cvGetFileNodeByName(fs, node, nodename));
}

FileNode FileNode::operator[](int i) const
{
    return isSeq() ? FileNode(fs, (CvFileNode*)cvGetSeqElem(node->data.seq, i)) :
        i == 0 ? *this : FileNode();
}

String FileNode::name() const
{
    const char* str;
    return !node || (str = cvGetFileNodeName(node)) == 0 ? String() : String(str);
}

void* FileNode::readObj() const
{
    if( !fs || !node )
        return 0;
    return cvRead( (CvFileStorage*)fs, (CvFileNode*)node );
}

static const FileNodeIterator::SeqReader emptyReader = {0, 0, 0, 0, 0, 0, 0, 0};

FileNodeIterator::FileNodeIterator()
{
    fs = 0;
    container = 0;
    reader = emptyReader;
    remaining = 0;
}

FileNodeIterator::FileNodeIterator(const CvFileStorage* _fs,
                                   const CvFileNode* _node, size_t _ofs)
{
    reader = emptyReader;
    if( _fs && _node && CV_NODE_TYPE(_node->tag) != CV_NODE_NONE )
    {
        int node_type = _node->tag & FileNode::TYPE_MASK;
        fs = _fs;
        container = _node;
        if( !(_node->tag & FileNode::USER) && (node_type == FileNode::SEQ || node_type == FileNode::MAP) )
        {
            cvStartReadSeq( _node->data.seq, (CvSeqReader*)&reader );
            remaining = FileNode(_fs, _node).size();
        }
        else
        {
            reader.ptr = (schar*)_node;
            reader.seq = 0;
            remaining = 1;
        }
        (*this) += (int)_ofs;
    }
    else
    {
        fs = 0;
        container = 0;
        remaining = 0;
    }
}

FileNodeIterator::FileNodeIterator(const FileNodeIterator& it)
{
    fs = it.fs;
    container = it.container;
    reader = it.reader;
    remaining = it.remaining;
}

FileNodeIterator& FileNodeIterator::operator ++()
{
    if( remaining > 0 )
    {
        if( reader.seq )
        {
            if( ((reader).ptr += (((CvSeq*)reader.seq)->elem_size)) >= (reader).block_max )
            {
                cvChangeSeqBlock( (CvSeqReader*)&(reader), 1 );
            }
        }
        remaining--;
    }
    return *this;
}

FileNodeIterator FileNodeIterator::operator ++(int)
{
    FileNodeIterator it = *this;
    ++(*this);
    return it;
}

FileNodeIterator& FileNodeIterator::operator --()
{
    if( remaining < FileNode(fs, container).size() )
    {
        if( reader.seq )
        {
            if( ((reader).ptr -= (((CvSeq*)reader.seq)->elem_size)) < (reader).block_min )
            {
                cvChangeSeqBlock( (CvSeqReader*)&(reader), -1 );
            }
        }
        remaining++;
    }
    return *this;
}

FileNodeIterator FileNodeIterator::operator --(int)
{
    FileNodeIterator it = *this;
    --(*this);
    return it;
}

FileNodeIterator& FileNodeIterator::operator += (int ofs)
{
    if( ofs == 0 )
        return *this;
    if( ofs > 0 )
        ofs = std::min(ofs, (int)remaining);
    else
    {
        size_t count = FileNode(fs, container).size();
        ofs = (int)(remaining - std::min(remaining - ofs, count));
    }
    remaining -= ofs;
    if( reader.seq )
        cvSetSeqReaderPos( (CvSeqReader*)&reader, ofs, 1 );
    return *this;
}

FileNodeIterator& FileNodeIterator::operator -= (int ofs)
{
    return operator += (-ofs);
}


FileNodeIterator& FileNodeIterator::readRaw( const String& fmt, uchar* vec, size_t maxCount )
{
    if( fs && container && remaining > 0 )
    {
        size_t elem_size, cn;
        getElemSize( fmt, elem_size, cn );
        CV_Assert( elem_size > 0 );
        size_t count = std::min(remaining, maxCount);

        if( reader.seq )
        {
            cvReadRawDataSlice( fs, (CvSeqReader*)&reader, (int)count, vec, fmt.c_str() );
            remaining -= count*cn;
        }
        else
        {
            cvReadRawData( fs, container, vec, fmt.c_str() );
            remaining = 0;
        }
    }
    return *this;
}


void write( FileStorage& fs, const String& name, int value )
{ cvWriteInt( *fs, name.size() ? name.c_str() : 0, value ); }

void write( FileStorage& fs, const String& name, float value )
{ cvWriteReal( *fs, name.size() ? name.c_str() : 0, value ); }

void write( FileStorage& fs, const String& name, double value )
{ cvWriteReal( *fs, name.size() ? name.c_str() : 0, value ); }

void write( FileStorage& fs, const String& name, const String& value )
{ cvWriteString( *fs, name.size() ? name.c_str() : 0, value.c_str() ); }

void writeScalar(FileStorage& fs, int value )
{ cvWriteInt( *fs, 0, value ); }

void writeScalar(FileStorage& fs, float value )
{ cvWriteReal( *fs, 0, value ); }

void writeScalar(FileStorage& fs, double value )
{ cvWriteReal( *fs, 0, value ); }

void writeScalar(FileStorage& fs, const String& value )
{ cvWriteString( *fs, 0, value.c_str() ); }


void write( FileStorage& fs, const String& name, const Mat& value )
{
    if( value.dims <= 2 )
    {
        CvMat mat = value;
        cvWrite( *fs, name.size() ? name.c_str() : 0, &mat );
    }
    else
    {
        CvMatND mat = value;
        cvWrite( *fs, name.size() ? name.c_str() : 0, &mat );
    }
}

// TODO: the 4 functions below need to be implemented more efficiently
void write( FileStorage& fs, const String& name, const SparseMat& value )
{
    Ptr<CvSparseMat> mat(cvCreateSparseMat(value));
    cvWrite( *fs, name.size() ? name.c_str() : 0, mat );
}


internal::WriteStructContext::WriteStructContext(FileStorage& _fs,
    const String& name, int flags, const String& typeName) : fs(&_fs)
{
    cvStartWriteStruct(**fs, !name.empty() ? name.c_str() : 0, flags,
                       !typeName.empty() ? typeName.c_str() : 0);
    fs->elname = String();
    if ((flags & FileNode::TYPE_MASK) == FileNode::SEQ)
    {
        fs->state = FileStorage::VALUE_EXPECTED;
        fs->structs.push_back('[');
    }
    else
    {
        fs->state = FileStorage::NAME_EXPECTED + FileStorage::INSIDE_MAP;
        fs->structs.push_back('{');
    }
}

internal::WriteStructContext::~WriteStructContext()
{
    cvEndWriteStruct(**fs);
    fs->structs.pop_back();
    fs->state = fs->structs.empty() || fs->structs.back() == '{' ?
        FileStorage::NAME_EXPECTED + FileStorage::INSIDE_MAP :
        FileStorage::VALUE_EXPECTED;
    fs->elname = String();
}


void read( const FileNode& node, Mat& mat, const Mat& default_mat )
{
    if( node.empty() )
    {
        default_mat.copyTo(mat);
        return;
    }
    void* obj = cvRead((CvFileStorage*)node.fs, (CvFileNode*)*node);
    if(CV_IS_MAT_HDR_Z(obj))
    {
        cvarrToMat(obj).copyTo(mat);
        cvReleaseMat((CvMat**)&obj);
    }
    else if(CV_IS_MATND_HDR(obj))
    {
        cvarrToMat(obj).copyTo(mat);
        cvReleaseMatND((CvMatND**)&obj);
    }
    else
    {
        cvRelease(&obj);
        CV_Error(CV_StsBadArg, "Unknown array type");
    }
}

void read( const FileNode& node, SparseMat& mat, const SparseMat& default_mat )
{
    if( node.empty() )
    {
        default_mat.copyTo(mat);
        return;
    }
    Ptr<CvSparseMat> m((CvSparseMat*)cvRead((CvFileStorage*)node.fs, (CvFileNode*)*node));
    CV_Assert(CV_IS_SPARSE_MAT(m));
    m->copyToSparseMat(mat);
}

CV_EXPORTS void read(const FileNode& node, KeyPoint& value, const KeyPoint& default_value)
{
    if( node.empty() )
    {
        value = default_value;
        return;
    }
    node >> value;
}

CV_EXPORTS void read(const FileNode& node, DMatch& value, const DMatch& default_value)
{
    if( node.empty() )
    {
        value = default_value;
        return;
    }
    node >> value;
}

#ifdef CV__LEGACY_PERSISTENCE
void write( FileStorage& fs, const String& name, const std::vector<KeyPoint>& vec)
{
    // from template implementation
    cv::internal::WriteStructContext ws(fs, name, FileNode::SEQ);
    write(fs, vec);
}

void read(const FileNode& node, std::vector<KeyPoint>& keypoints)
{
    FileNode first_node = *(node.begin());
    if (first_node.isSeq())
    {
        // modern scheme
#ifdef OPENCV_TRAITS_ENABLE_DEPRECATED
        FileNodeIterator it = node.begin();
        size_t total = (size_t)it.remaining;
        keypoints.resize(total);
        for (size_t i = 0; i < total; ++i, ++it)
        {
            (*it) >> keypoints[i];
        }
#else
        FileNodeIterator it = node.begin();
        it >> keypoints;
#endif
        return;
    }
    keypoints.clear();
    FileNodeIterator it = node.begin(), it_end = node.end();
    for( ; it != it_end; )
    {
        KeyPoint kpt;
        it >> kpt.pt.x >> kpt.pt.y >> kpt.size >> kpt.angle >> kpt.response >> kpt.octave >> kpt.class_id;
        keypoints.push_back(kpt);
    }
}

void write( FileStorage& fs, const String& name, const std::vector<DMatch>& vec)
{
    // from template implementation
    cv::internal::WriteStructContext ws(fs, name, FileNode::SEQ);
    write(fs, vec);
}

void read(const FileNode& node, std::vector<DMatch>& matches)
{
    FileNode first_node = *(node.begin());
    if (first_node.isSeq())
    {
        // modern scheme
#ifdef OPENCV_TRAITS_ENABLE_DEPRECATED
        FileNodeIterator it = node.begin();
        size_t total = (size_t)it.remaining;
        matches.resize(total);
        for (size_t i = 0; i < total; ++i, ++it)
        {
            (*it) >> matches[i];
        }
#else
        FileNodeIterator it = node.begin();
        it >> matches;
#endif
        return;
    }
    matches.clear();
    FileNodeIterator it = node.begin(), it_end = node.end();
    for( ; it != it_end; )
    {
        DMatch m;
        it >> m.queryIdx >> m.trainIdx >> m.imgIdx >> m.distance;
        matches.push_back(m);
    }
}
#endif

int FileNode::type() const { return !node ? NONE : (node->tag & TYPE_MASK); }
bool FileNode::isNamed() const { return !node ? false : (node->tag & NAMED) != 0; }

size_t FileNode::size() const
{
    int t = type();
    return t == MAP ? (size_t)((CvSet*)node->data.map)->active_count :
        t == SEQ ? (size_t)node->data.seq->total : (size_t)!isNone();
}

void read(const FileNode& node, int& value, int default_value)
{
    value = !node.node ? default_value :
    CV_NODE_IS_INT(node.node->tag) ? node.node->data.i : std::numeric_limits<int>::max();
}

void read(const FileNode& node, float& value, float default_value)
{
    value = !node.node ? default_value :
        CV_NODE_IS_INT(node.node->tag) ? (float)node.node->data.i :
        CV_NODE_IS_REAL(node.node->tag) ? saturate_cast<float>(node.node->data.f) : std::numeric_limits<float>::max();
}

void read(const FileNode& node, double& value, double default_value)
{
    value = !node.node ? default_value :
        CV_NODE_IS_INT(node.node->tag) ? (double)node.node->data.i :
        CV_NODE_IS_REAL(node.node->tag) ? node.node->data.f : std::numeric_limits<double>::max();
}

void read(const FileNode& node, String& value, const String& default_value)
{
    value = !node.node ? default_value : CV_NODE_IS_STRING(node.node->tag) ? String(node.node->data.str.ptr) : String();
}

void read(const FileNode& node, std::string& value, const std::string& default_value)
{
    value = !node.node ? default_value : CV_NODE_IS_STRING(node.node->tag) ? std::string(node.node->data.str.ptr) : default_value;
}

}
















/* End of file. */
