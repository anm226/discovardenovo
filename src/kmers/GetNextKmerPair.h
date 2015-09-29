// Copyright (c) 2000-2003 Whitehead Institute for Biomedical Research

#ifndef GETNEXTKMERPAIR
#define GETNEXTKMERPAIR

#include <algorithm>
#include "CoreTools.h"
#include "kmers/KmerRecord.h"

// GetNextKmerPair: given a sorted vector of kmer_records, as would be produced by
// SortKmers, produce a datum (id1, pos1, id2, pos2) corresponding to each pair with
// the same k-mer.  The positions are negated in the case of a reverse complement.

// The first time, call with read_id1 = -1.  When there are no more data left,
// the routine exits with read_id1 = -1.

// Record sets having the same k-mer and of size > max_set are ignored.

// Warning.  This is not yet parallel-ready.  The static variables i,j,r1,r2 need 
// to have separate values for each caller.

// If avoid_promiscuous_kmers is True, record sets whose k-mer consists entirely of 
// the same base (repeated), or which do so with one exception, are now ignored, 
// when 4 | K.

// Note: There are two versions.  The second accepts an additional argument N0.

extern vec<longlong> kmer_multiplicity;
extern vec<unsigned short> high_mult_kmers;

inline vec<longlong>* KmerMultiplicities( ) { return &kmer_multiplicity; }

inline vec<unsigned short>* HighMultKmers( ) { return &high_mult_kmers; }



// The lookup table used by  GetNextKmerPair.

static const
unsigned char
GetNextKmerPair_four_bases[] =
{
  0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 
  0x00, 0x01, 0x01, 0x01, 0x00, 0x01, 0x01, 0x01, 
  0x00, 0x01, 0x01, 0x01, 0x01, 0x55, 0x01, 0x01, 
  0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 
  0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 
  0x01, 0x01, 0xaa, 0x01, 0x01, 0x01, 0x01, 0x01, 
  0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 
  0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0xff, 
  0x00, 0x01, 0x01, 0x01, 0x01, 0x55, 0x01, 0x01, 
  0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 
  0x01, 0x55, 0x01, 0x01, 0x55, 0x55, 0x55, 0x55, 
  0x01, 0x55, 0x01, 0x01, 0x01, 0x55, 0x01, 0x01, 
  0x01, 0x01, 0x01, 0x01, 0x01, 0x55, 0x01, 0x01, 
  0x01, 0x01, 0xaa, 0x01, 0x01, 0x01, 0x01, 0x01, 
  0x01, 0x01, 0x01, 0x01, 0x01, 0x55, 0x01, 0x01, 
  0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0xff, 
  0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 
  0x01, 0x01, 0xaa, 0x01, 0x01, 0x01, 0x01, 0x01, 
  0x01, 0x01, 0x01, 0x01, 0x01, 0x55, 0x01, 0x01, 
  0x01, 0x01, 0xaa, 0x01, 0x01, 0x01, 0x01, 0x01, 
  0x01, 0x01, 0xaa, 0x01, 0x01, 0x01, 0xaa, 0x01, 
  0xaa, 0xaa, 0xaa, 0xaa, 0x01, 0x01, 0xaa, 0x01, 
  0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 
  0x01, 0x01, 0xaa, 0x01, 0x01, 0x01, 0x01, 0xff, 
  0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 
  0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0xff, 
  0x01, 0x01, 0x01, 0x01, 0x01, 0x55, 0x01, 0x01, 
  0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0xff, 
  0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 
  0x01, 0x01, 0xaa, 0x01, 0x01, 0x01, 0x01, 0xff, 
  0x01, 0x01, 0x01, 0xff, 0x01, 0x01, 0x01, 0xff, 
  0x01, 0x01, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff
};


// The table was generated by the following code:
//
//   unsigned char four_bases[256];
//   for ( int v = 0; v < 256; v++ )
//     four_bases[v] = 1;
//   for ( unsigned char m = 0; m < 4; m++ )
//     {    unsigned char NNNN = (m<<0) ^ (m<<2) ^ (m<<4) ^ (m<<6);
//     for ( unsigned char i = 0; i < 4; i++ )
//       for ( unsigned char j = 0; j < 8; j += 2 )
// 	four_bases[ NNNN ^ (i << j) ] = NNNN;    }
// 
//   cout << "static const" << endl;
//   cout << "unsigned char" << endl;
//   cout << "GetNextKmerPair_four_bases[] =" << endl;
//   cout << "{" << endl;
// 
//   for (int index = 0; index < 256; index++)
//     {
//       if (index % 8 == 0)
// 	cout << "  ";
// 
//       cout << "0x"
// 	   << setw(2) << setfill('0') << hex
// 	   << static_cast<unsigned int>(four_bases[index])
// 	   << ", ";
// 
//       if (index % 8 == 7)
// 	cout << endl;
//     }
// 
//   cout << "};" << endl;

//------------------------------------------------------------


template<int K, int I> 
     inline void GetNextKmerPair( const vec< kmer_record<K,I> >& R, int S,
     int& read_id1, int& read_id2, int& pos1, int& pos2, int max_set,
     vec<int>& multiplicities, Bool use_multiplicities = False, 
     Bool avoid_promiscuous_kmers = False )
{    static int i, j, r1, r2;
     if ( read_id1 >= 0 ) goto after_return;
     for ( i = 0; i < S; i++ )
     {    // Find the first kmer_record whose K-mer entry differs from the ith entry.
          int l;
          for ( j = i+1; j < S; j++ )
          {    for ( l = (K+3)/4 - 1; l >= 0; l-- )
                    if ( R[j].Bytes( )[l] != R[i].Bytes( )[l] ) break;
               if ( l >= 0 ) break;    }
          if (use_multiplicities) multiplicities.push_back(j-i);
          if ( j - i >= (int) kmer_multiplicity.size( ) )
          {    int old_size = kmer_multiplicity.size( );
               kmer_multiplicity.resize( j - i + 2000 );
               for ( int l = old_size; l < (int) kmer_multiplicity.size( ); l++ )
                    kmer_multiplicity[l] = 0;    }
          ++kmer_multiplicity[j - i];
          if ( j - i > max_set )
          {    if ( high_mult_kmers.size( ) > 0 )
               {    for ( int l = i; l < j; l++ )
                    {    int r = R[l].GetId( );
                         if ( high_mult_kmers[r] < 65535 ) 
                              ++high_mult_kmers[r];    }    }
               i = j - 1;
               continue;    }

          if ( avoid_promiscuous_kmers && K % 4 == 0 )
          {    const unsigned char* r = R[i].Bytes( );
               int v, diffs = 0;
               for ( v = 0; v < (K+3)/4; v++ )
               {    if ( GetNextKmerPair_four_bases[ r[v] ] == 1 ) break;
                    if ( v > 0 && GetNextKmerPair_four_bases[ r[v-1] ] != GetNextKmerPair_four_bases[ r[v] ] ) break;
                    if ( GetNextKmerPair_four_bases[ r[v] ] != r[v] ) ++diffs;    }
               if ( v == (K+3)/4 && diffs <= 1 ) 
               {    
                    // cout << "rejecting ";
                    // for ( int u = 0; u < (K+3)/4; u++ )
                    // {    for ( int w = 0; w < 8; w += 2 )
                    //           cout << int(((r[u] >> w) & 3));    }
                    // cout << "\n";

                    i = j - 1;
                    continue;    }    }

          // Now extract pairs of read_id's and pos's from kmer_records i..j-1.
          for ( r1 = i; r1 < j; r1++ )
          {    read_id1 = R[r1].GetId( );
               pos1 = R[r1].GetPos( );
               for ( r2 = r1+1; r2 < j; r2++ )
               {    read_id2 = R[r2].GetId( );
                    if ( read_id1 == read_id2 ) continue;
                    pos2 = R[r2].GetPos( );
                    return;
                    after_return: continue;    }    }    
          i = j - 1;    }    
     read_id1 = -1;    }

template<int K, int I> 
     inline void GetNextKmerPair( int N0, vec< kmer_record<K,I> >& R, int S,
     int& read_id1, int& read_id2, int& pos1, int& pos2, int max_set,
     vec<int>& multiplicities, Bool use_multiplicities = False, 
     Bool avoid_promiscuous_kmers = False )
{    static int i, j, r1, r2, first_ge_N0;
     if ( read_id1 >= 0 ) goto after_return;
     for ( i = 0; i < S; i++ )
     {    // Find the first kmer_record whose K-mer entry differs from the ith entry.
          int l;
          for ( j = i+1; j < S; j++ )
          {    for ( l = (K+3)/4 - 1; l >= 0; l-- )
                    if ( R[j].Bytes( )[l] != R[i].Bytes( )[l] ) break;
               if ( l >= 0 ) break;    }
          if (use_multiplicities) multiplicities.push_back(j-i);
          if ( j - i >= (int) kmer_multiplicity.size( ) )
          {    int old_size = kmer_multiplicity.size( );
               kmer_multiplicity.resize( j - i + 2000 );
               for ( int l = old_size; l < (int) kmer_multiplicity.size( ); l++ )
                    kmer_multiplicity[l] = 0;    }
          ++kmer_multiplicity[j - i];
          if ( j - i > max_set )
          {    if ( high_mult_kmers.size( ) > 0 )
               {    for ( int l = i; l < j; l++ )
                    {    int r = R[l].GetId( );
                         if ( high_mult_kmers[r] < 65535 ) 
                              ++high_mult_kmers[r];    }    }
               i = j - 1;
               continue;    }

          if ( avoid_promiscuous_kmers && K % 4 == 0 )
          {    const unsigned char* r = R[i].Bytes( );
               int v, diffs = 0;
               for ( v = 0; v < (K+3)/4; v++ )
               {    if ( GetNextKmerPair_four_bases[ r[v] ] == 1 ) break;
                    if ( v > 0 && GetNextKmerPair_four_bases[ r[v-1] ] != GetNextKmerPair_four_bases[ r[v] ] ) break;
                    if ( GetNextKmerPair_four_bases[ r[v] ] != r[v] ) ++diffs;    }
               if ( v == (K+3)/4 && diffs <= 1 ) 
               {    
                    // cout << "rejecting ";
                    // for ( int u = 0; u < (K+3)/4; u++ )
                    // {    for ( int w = 0; w < 8; w += 2 )
                    //           cout << int(((r[u] >> w) & 3));    }
                    // cout << "\n";

                    i = j - 1;
                    continue;    }    }

          for ( r1 = i; r1 < j; r1++ )
               if ( R[r1].GetId( ) >= N0 ) break;
          if ( r1 == j )
          {    i = j - 1;
               continue;    }

          sort( R.begin( ) + i, R.begin( ) + j, kmer_record<K,I>::id_cmp );
          for ( r1 = i; r1 < j; r1++ )
               if ( R[r1].GetId( ) >= N0 )
               {    first_ge_N0 = r1;
                    break;    }
          if ( first_ge_N0 == i )
          {    i = j - 1;
               continue;    }

          // Now extract pairs of read_id's and pos's from kmer_records i..j-1.
          for ( r1 = i; r1 < first_ge_N0; r1++ )
          {    read_id1 = R[r1].GetId( );
               pos1 = R[r1].GetPos( );
               for ( r2 = first_ge_N0; r2 < j; r2++ )
               {    read_id2 = R[r2].GetId( );
                    pos2 = R[r2].GetPos( );
                    return;
                    after_return: continue;    }    }    
          i = j - 1;    }    
     read_id1 = -1;    }

#endif