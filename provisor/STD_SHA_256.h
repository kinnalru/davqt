//#############################################################################
//##  File Name:        STD_SHA_256.h                                        ##
//##  File Version:     1.00 (19 Jul 2008)                                   ##
//##  Author:           Silvestro Fantacci                                   ##
//##  Copyright:        Public Domain                                        ##
//#############################################################################
//
//  THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
//  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
//  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//  DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
//  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
//  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
//  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
//  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
//  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
//  IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
//  POSSIBILITY OF SUCH DAMAGE.

#if !defined (STD_SHA_256_INCLUDED)
#define STD_SHA_256_INCLUDED
#define __int8 __UINT8_TYPE__
#define __int32 __UINT32_TYPE__
#define __int64 __UINT64_TYPE__

#include <string>


//=============================================================================
class STD_SHA_256
        //=============================================================================
        // This class implements a SHA-256 secure hash algorithm as specified in the
        // "Secure Hash Standard (SHS)" document FIPS PUB 180-2, August 2002.
        // The SHA-256 algorithm calculates a 32 byte (256 bit) number - called a
        // "message digest" - from an input message of any length from 0 to 2^64 - 1
        // bits.
        // The algorithm implemented in this class is byte oriented, i.e. the input
        // message resolution is in bytes and its length can range from 0 to 2^61 - 1
        // bytes.
{
public:

    //--------  Two Phase Calculation  --------

    // Phase 1 - Add the whole message, byte by byte
    // Phase 2 - Inspect the calculated digest

    // Note: calls to the Digest methods are taken to imply that the whole
    // message has been added. Hence a subsequent call to Add, if any, will
    // be interpreted as the restart of Phase 1 on a new message.

    STD_SHA_256 ();

    void				Add ( __int8 Byte);

    // Index in 0 .. 31
    __int8		Digest (unsigned int Index);

    // Returns the digest's hexadecimal representation.
    // 64 charactes long, A to F uppercase.
    std::string			Digest ();

    // To explicitly request the restart of Phase 1 on a new message.
    void				Reset ();

    virtual				~STD_SHA_256 ();

    //--------  Oneshot Calculations  --------

    // Message_Length is in bytes
    static void			Get_Digest (void *				Message,
                                    unsigned int		Message_Length,
                                    __int8		Digest [32]);

    // The digest's hexadecimal representation.
    // The string is 64 charactes long, with A to F uppercase.
    // E.g. Digest ("abc") == "BA7816BF8F01CFEA414140DE5DAE2223"
    //                        "B00361A396177A9CB410FF61F20015AD"
    static std::string	Digest (std::string Message);

private:

    static __int32	Rotated_Right (__int32		Word,
                                           unsigned int			Times);

    static __int32	Shifted_Right (__int32		Word,
                                           unsigned int			Times);

    static __int32	Upper_Sigma_0 (__int32	Word);

    static __int32	Upper_Sigma_1 (__int32	Word);

    static __int32	Lower_Sigma_0 (__int32	Word);

    static __int32	Lower_Sigma_1 (__int32	Word);

    static __int32	Ch (__int32 Word_1,
                                __int32 Word_2,
                                __int32 Word_3);

    static __int32	Maj (__int32 Word_1,
                                 __int32 Word_2,
                                 __int32 Word_3);

    void					Padd ();

    void					Process_Block ();

    // A message block of 64 * 8 = 512 bit
    __int8			my_Block [64];

    __int32		my_Hash_Value [8];
    __int64		my_Total_Bytes;
    bool					my_Padding_is_Done;

    static const __int32	K [64];
};

#endif // !defined (STD_SHA_256_INCLUDED)
