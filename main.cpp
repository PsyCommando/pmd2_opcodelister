#include <iostream>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <cassert>
#include <iomanip>
using namespace std;


/*********************************************************************************************
    ReadIntFromBytes
        Tool to read integer values from a byte vector!
        ** The iterator's passed as input, has its position changed !!
*********************************************************************************************/
template<class T, class _init> 
    inline T ReadIntFromBytes( _init & itin, _init itend, bool basLittleEndian = true )
{
    static_assert( std::numeric_limits<T>::is_integer, "ReadIntFromBytes() : Type T is not an integer!" );
    T out_val = 0;

    if( basLittleEndian )
    {
        unsigned int i = 0;
        for( ; (itin != itend) && (i < sizeof(T)); ++i, ++itin )
        {
            T tmp = (*itin);
            out_val |= ( tmp << (i * 8) ) & ( 0xFF << (i*8) );
        }

        if( i != sizeof(T) )
        {
#ifdef _DEBUG
            assert(false);
#endif
            throw std::runtime_error( "ReadIntFromBytes(): Not enough bytes to read from the source container!" );
        }
    }
    else
    {
        int i = (sizeof(T)-1);
        for( ; (itin != itend) && (i >= 0); --i, ++itin )
        {
            T tmp = (*itin);
            out_val |= ( tmp << (i * 8) ) & ( 0xFF << (i*8) );
        }

        if( i != -1 )
        {
#ifdef _DEBUG
            assert(false);
#endif
            throw std::runtime_error( "ReadIntFromBytes(): Not enough bytes to read from the source container!" );
        }
    }
    return out_val;
}

/*********************************************************************************************
    ReadIntFromBytes
        Tool to read integer values from a byte container!
            
        #NOTE :The iterator is passed by copy here !! And the incremented iterator is returned!
*********************************************************************************************/
template<class T, class _init> 
    inline _init ReadIntFromBytes( T & dest, _init itin, _init itend, bool basLittleEndian = true ) 
{
    dest = ReadIntFromBytes<typename T, typename _init>( itin, itend, basLittleEndian );
    return itin;
}

/************************************************************************************
    opcodetblentry_eos
        Contain opcode data from the opcode table.
************************************************************************************/
struct alignas(8) opcodetblentry_eos
{
    int8_t   nbparams;
    int8_t   unk1;
    int8_t   unk2;
    int8_t   unk3;
    uint32_t stringoffset;
};

/************************************************************************************
    opcodetblentry_eot
        Contain opcode data from the opcode table.
************************************************************************************/
struct opcodetblentry_eot
{
    int8_t   nbparams;
    uint32_t stringoffset;
};

/************************************************************************************
    safestrlen
        Count the length of a string, and has a iterator check
        to ensure it won't loop into infinity if it can't find a 0.
************************************************************************************/
template<typename init_t>
    inline size_t safestrlen( init_t beg, init_t pastend )
{
    size_t cntchar = 0;
    for(; beg != pastend && (*beg) != 0; ++cntchar, ++beg );

    if( beg == pastend )
        throw runtime_error("String went past expected end!");

    return cntchar;
}

/************************************************************************************
    LoadFile
        Load a file into a byte vector for easier parsing.
************************************************************************************/
std::vector<uint8_t> LoadFile( const std::string & fpath )
{
    ifstream fi( fpath, ios::binary );
    if( fi.bad() || !(fi.is_open()) )
        throw std::runtime_error("Couldn't open file " + fpath);

    return std::move( std::vector<uint8_t>( istreambuf_iterator<char>(fi), istreambuf_iterator<char>()) );
}

/************************************************************************************
    FetchString
        Fetchs a null terminated C-String from a file offset.
************************************************************************************/
template<typename _init>
    std::string FetchString( uint32_t fileoffset, _init itfbeg, _init itfend )
{
    auto    itstr = itfbeg;
    std::advance( itstr,  fileoffset );
    size_t  strlength = safestrlen(itstr, itfend);
    string  dest;
    dest.resize(strlength);

    for( size_t cntchar = 0; cntchar < strlength; ++cntchar, ++itstr )
        dest[cntchar] = (*itstr);

    return std::move(dest);
}

/************************************************************************************
    DumpEoS
        Dumps the opcode table and nb of parameters, and etc, for Explorers of Sky!
************************************************************************************/
void DumpEoS( const string & overlayfname, const string & outfname )
{
    static const size_t         NBOpcodes   = 383;       //Nb of entries in the opcode table.
    static const uint32_t       OffsetTable = 0x3C3D0;   //Offset of the opcode table in the overlay_0011 file for EoS. 
    static const uint32_t       DiffPointer = 0x22DC240; //Difference between memory space and overlay file space for pointer addresses

    vector<uint8_t>             fdat( LoadFile(overlayfname) );
    auto                        itbeg = fdat.begin();
    auto                        itend = fdat.end();
    vector<opcodetblentry_eos>  entries(NBOpcodes, opcodetblentry_eos{0,0,0,0,0} );
    vector<string>              opnames(NBOpcodes);

    cout<<"Started EoS!\n";
    //Advance iterator to beginning of opcode table
    std::advance(itbeg, OffsetTable);
    
    //### Phase 1: Load up the table ###
    for( size_t i = 0; i < NBOpcodes; ++i )
    {
        //Get the entry from the table
        auto & entry = entries[i];
        entry.nbparams = (*itbeg);
        ++itbeg;
        entry.unk1     = (*itbeg);
        ++itbeg;
        entry.unk2     = (*itbeg);
        ++itbeg;
        entry.unk3     = (*itbeg);
        ++itbeg;
        entry.stringoffset = ReadIntFromBytes<uint32_t>(itbeg, itend);
        
        //Scoop up the string from the string table
        uint32_t  straddr = entry.stringoffset - DiffPointer;
        opnames[i]        = std::move( FetchString(straddr, fdat.begin(), itend) );

        cout <<"\rRead " <<setfill(' ') <<setw(3) <<left <<i + 1 ;
    }
    
    //### Phase 2: Dump everything to a text file ###
    //* I could have done this directly in the first loop, but its probably better to keep those separated 
    //  in case I want to re-use this.
    cout<<"\nWriting EoS!\n";
    ofstream            of(outfname);
    static const size_t LongestString = 36; //Calculate that dynamically one day maybe

    of << "=============================================================\n"
       << "\tScript OpCode List\n"
       << "=============================================================\n";

    //Dump to output
    for( size_t i = 0; i < NBOpcodes; ++i )
    {
        stringstream sstr;
        sstr <<"\t0x" <<hex <<setfill('0') <<setw(3) <<right <<i <<dec 
            <<" - " <<setfill(' ') <<setw(LongestString) <<left <<opnames[i] + "," 
            <<setfill(' ') <<setw(2) <<right
            <<static_cast<short>(entries[i].nbparams)
            <<" params, Unk1: " <<setfill(' ') <<setw(3) <<right  <<static_cast<short>(entries[i].unk1) 
            <<", Unk2: "        <<setfill(' ') <<setw(3) <<right <<static_cast<short>(entries[i].unk2) 
            <<", Unk3: "        <<setfill(' ') <<setw(3) <<right <<static_cast<short>(entries[i].unk3) <<dec <<"\n";
        of << sstr.str();
    }

    cout <<"\nDone!\n";
}

/************************************************************************************
    DumpEoT
        Dumps the opcode table and nb of parameters for Explorers of Time!
************************************************************************************/
void DumpEoT( const string & overlayfname, const string & outfname )
{
    static const size_t         NBOpcodes            = 336;       //Nb of entries in the opcode table.
    static const uint32_t       OffsetStringPtrTable = 0x5CAAC;   //
    static const uint32_t       OffsetParamTable     = 0x509C0;   //Offset in the overlay of the table containing the nb of params for each command
    static const uint32_t       OverlayLoadOffset    = 0x22D5B20;

    vector<uint8_t>             fdat( LoadFile(overlayfname) );
    auto                        itbeg = fdat.begin();
    auto                        itend = fdat.end();
    vector<opcodetblentry_eot>  entries(NBOpcodes, opcodetblentry_eot{0,0} );
    vector<string>              opnames(NBOpcodes);

    cout<<"Started EoT!\n";
    //Advance iterator to beginning of string ptr table
    auto itbegptr = itbeg;
    std::advance(itbegptr, OffsetStringPtrTable);
    //Place an iterator to the start of the parameter nb table
    auto itbegparams = itbeg;
    std::advance(itbegparams, OffsetParamTable);

    //### Phase 1: Load up the data ###
    for( size_t i = 0; i < NBOpcodes; ++i )
    {
        //Get the data
        entries[i].stringoffset = ReadIntFromBytes<uint32_t>( itbegptr, itend );    //iterator incremented
        entries[i].nbparams     = ReadIntFromBytes<int8_t>  ( itbegparams, itend ); //iterator incremented

        //Get the string
        uint32_t stroffset = entries[i].stringoffset - OverlayLoadOffset;
        opnames[i]         = std::move( FetchString(stroffset, fdat.begin(), itend) );

        cout <<"\rRead " <<setfill(' ') <<setw(3) <<left <<i + 1 ;
    }

    //### Phase 2: Dump everything to a text file ###
    //* I could have done this directly in the first loop, but its probably better to keep those separated 
    //  in case I want to re-use this.
    cout<<"\nWriting EoT!\n";
    ofstream of(outfname);
    static const size_t LongestString = 36; //Calculate that dynamically one day maybe

    of << "=============================================================\n"
       << "\tScript OpCode List for EoT\n"
       << "=============================================================\n";

    //Dump to output
    for( size_t i = 0; i < NBOpcodes; ++i )
    {
        stringstream sstr;
        sstr <<"\t0x" <<hex <<setfill('0') <<setw(3) <<right <<i <<dec <<" - " 
            <<setfill(' ') <<setw(LongestString) <<left <<opnames[i] + ","
            <<setfill(' ') <<setw(2) <<right
            <<static_cast<short>(entries[i].nbparams) <<"\n";
        of << sstr.str();
    }

    cout <<"\nDone!\n";

}

/************************************************************************************
    main
        This is where all the work is done.
************************************************************************************/
int main( int argc, const char * argv[] )
{
    DumpEoS( "overlay_0011.bin", "opcodelist_eos.txt" );
    DumpEoT( "overlay_0004.bin", "opcodelist_eot.txt" );
    return 0;
}