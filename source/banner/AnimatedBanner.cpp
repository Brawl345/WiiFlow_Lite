/*
Copyright (c) 2010 - Wii Banner Player Project
Copyright (c) 2012 - Dimok

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
claim that you wrote the original software. If you use this software
in a product, an acknowledgment in the product documentation would be
appreciated but is not required.

2. Altered source versions must be plainly marked as such, and must not be
misrepresented as being the original software.

3. This notice may not be removed or altered from any source
distribution.
*/
#include <malloc.h>
#include "U8Archive.h"
#include "LanguageCode.h"
#include "AnimatedBanner.h"
#include "text.hpp"
#include "lz77.h"
#include "ash.h"

AnimatedBanner::AnimatedBanner(u8 *font1, u8 *font2)
{
	first = true;
	layout_banner = NULL;
	newBanner = NULL;
	sysFont1 = font1;
	sysFont2 = font2;
}

void AnimatedBanner::Clear()
{
	if(first)
		return;
	if(layout_banner)
	{
		delete layout_banner;
		layout_banner = NULL;
	}
	if(newBanner)
	{
		delete newBanner;
		newBanner = NULL;
	}
}

bool AnimatedBanner::LoadBanner(Banner *banner)
{
	first = false;
	Clear();

	u32 banner_bin_size;
	const u8 *banner_bin = banner->GetFile((char*)"banner.bin", &banner_bin_size);
	if(banner_bin == NULL)
		return false;

	layout_banner = LoadLayout(banner_bin, banner_bin_size, "banner", CONF_GetLanguageString());
	return (layout_banner != NULL);
}

Layout* AnimatedBanner::LoadLayout(const u8 *bnr, u32 bnr_size, const std::string& lyt_name, const std::string &language)
{
	u32 brlyt_size = 0;
	newBanner = DecompressCopy(bnr, bnr_size, &bnr_size);

	const u8 *brlyt = u8_get_file(newBanner, (char*)fmt("%s.brlyt", lyt_name.c_str()), &brlyt_size);
	if(!brlyt)
		return NULL;

	Layout *layout = new Layout(sysFont1, sysFont2);
	layout->Load(brlyt);

	u32 length_start = 0, length_loop = 0;

	u32 brlan_start_size = 0;
	const u8 *brlan_start = u8_get_file(newBanner, (char*)fmt("%s_Start.brlan", lyt_name.c_str()), &brlan_start_size);
	const u8 *brlan_loop = 0;

	// try the alternative file
	if(!brlan_start)
		brlan_start = u8_get_file(newBanner, (char*)fmt("%s_In.brlan", lyt_name.c_str()), &brlan_start_size);

	if(brlan_start)
		length_start = Animator::LoadAnimators((const RLAN_Header *)brlan_start, *layout, 0);

	u32 brlan_loop_size = 0;
	brlan_loop = u8_get_file(newBanner, (char*)fmt("%s.brlan", lyt_name.c_str()), &brlan_loop_size);
	if(!brlan_loop)
		brlan_loop = u8_get_file(newBanner, (char*)fmt("%s_Loop.brlan", lyt_name.c_str()), &brlan_loop_size);
	if(!brlan_loop)
		brlan_loop = u8_get_file(newBanner, (char*)fmt("%s_Rso0.brlan", lyt_name.c_str()), &brlan_loop_size); // added for "artstyle" wiiware

	if(brlan_loop)
		length_loop = Animator::LoadAnimators((const RLAN_Header *)brlan_loop, *layout, 1);

	// load textures after loading the animations so we get the list of tpl filenames from the brlans
	layout->LoadTextures(newBanner);
	layout->LoadFonts(newBanner);
	layout->SetLanguage(language);
	layout->SetLoopStart(length_start);
	layout->SetLoopEnd(length_start + length_loop);
	layout->SetFrame(0);

	return layout;
}

u8 *AnimatedBanner::DecompressCopy( const u8 * stuff, u32 len, u32 *size )
{
	// check for IMD5 header and skip it
	if( len > 0x40 && *(u32*)stuff == 0x494d4435 )// IMD5
	{
		stuff += 0x20;
		len -= 0x20;
	}

	u8* ret = NULL;
	// determine if it needs to be decompressed
	if( IsAshCompressed( stuff, len ) )
	{
		//u32 len2 = len;
		// ASH0
		ret = DecompressAsh( stuff, len );
		if( !ret )
		{
			gprintf( "out of memory\n" );
			return NULL;
		}
	}
	else if( isLZ77compressed( (u8*)stuff ) )
	{
		// LZ77 with no magic word
		if( decompressLZ77content( (u8*)stuff, len, &ret, &len ) )
		{
			return NULL;
		}
	}
	else if( *(u32*)( stuff ) == 0x4C5A3737 )// LZ77
	{
		// LZ77 with a magic word
		if( decompressLZ77content( (u8*)stuff + 4, len - 4, &ret, &len ) )
		{
			return NULL;
		}
	}
	else
	{
		// just copy the data out of the archive
		ret = (u8*)memalign( 32, len );
		if( !ret )
		{
			gprintf( "out of memory\n" );
			return NULL;
		}
		memcpy( ret, stuff, len );
	}
	if( size )
	{
		*size = len;
	}

	// flush the cache so if there are any textures in this data, it will be ready for the GX
	DCFlushRange( ret, len );
	return ret;
}