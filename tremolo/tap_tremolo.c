/*                                                     -*- linux-c -*-
    Copyright (C) 2004 Tom Szilagyi

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    $Id: tap_tremolo.c,v 1.6 2004/02/21 17:33:36 tszilagyi Exp $
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>

#include "lv2.h"
#include "tap_utils.h"

/* The Unique ID of the plugin: */

#define ID_MONO         2144

/* The port numbers for the plugin: */

#define CONTROL_FREQ    0
#define CONTROL_DEPTH   1
#define CONTROL_GAIN    2
#define CONTROL_TAP     3
#define INPUT_0         4
#define OUTPUT_0        5


/* Total number of ports */

#define PORTCOUNT_MONO   6


/* cosine table for fast computations */
float cos_table[1024];
int flag = 0;



/* The structure used to hold port connection information and state */

typedef struct {
	float * Control_Freq;
	float * Control_Depth;
	float * Control_Gain;
	float * Control_Tap;
	float oldgain;
	float * InputBuffer_1;
	float * OutputBuffer_1;
	double SampleRate;
	float Phase;
	int start_tap;      ///< when did the last tap happen (ms since epoch)
} Tremolo;



/* Construct a new plugin instance. */
LV2_Handle
instantiate_Tremolo(const LV2_Descriptor * Descriptor, double SampleRate, const char* bundle_path, const LV2_Feature* const* features) {

	LV2_Handle * ptr;
	int i;

	if ((ptr = malloc(sizeof(Tremolo))) != NULL) {
	        ((Tremolo *)ptr)->SampleRate = SampleRate;
					((Tremolo *)ptr)->oldgain = 0.0f;
	        if(flag == 0)
	        {
                for(i = 0; i < 1024; i++)
                {
                    cos_table[i] = cosf(i * M_PI / 512.0f);
                    flag++;

	            }
            }
		return ptr;
	}

	return NULL;
}

void
activate_Tremolo(LV2_Handle Instance) {

	Tremolo * ptr;

	ptr = (Tremolo *)Instance;
	ptr->Phase = 0.0f;
	// Reset tapping
    ptr->start_tap = 0;
}

/**
* Handles a tap on the tap button and calculates time differences
* \param self pointer to current plugin instance
* \return Beats per minute or zero if it didn't work
*/
static float handle_tap(Tremolo* self) {

    float d = 0;

    struct timeval t_cur;
    gettimeofday(&t_cur, 0);

    // convert it to milliseconds
    long int t_cur_ms = floor(t_cur.tv_sec * 1000 + t_cur.tv_usec / 1000);

    // If start tap is memorized, do some calculations
    if (self->start_tap) {
        d = t_cur_ms - self->start_tap;

        // Reset if we exceed the maximum delay time
        if (d <= 50 || d > 10000 ) {
            d = 0;
        }
    }
    self->start_tap = t_cur_ms;
    return (d > 0 ? 60000 / d : 0);   // convert to bpm
}

void
deactivate_Tremolo(LV2_Handle Instance) {


}


/* Connect a port to a data location. */
void
connect_port_Tremolo(LV2_Handle Instance,
		     uint32_t Port,
		     void * DataLocation) {

	Tremolo * ptr;

	ptr = (Tremolo *)Instance;
	switch (Port) {
	case CONTROL_FREQ:
		ptr->Control_Freq = (float*) DataLocation;
		break;
	case CONTROL_DEPTH:
		ptr->Control_Depth = (float*) DataLocation;
		break;
	case CONTROL_GAIN:
		ptr->Control_Gain = (float*) DataLocation;
		break;
	case CONTROL_TAP:
		ptr->Control_Tap = (float*) DataLocation;
		break;
	case INPUT_0:
		ptr->InputBuffer_1 = (float*) DataLocation;
		break;
	case OUTPUT_0:
		ptr->OutputBuffer_1 = (float*) DataLocation;
		break;
	}
}



void
run_Tremolo(LV2_Handle Instance,
	    uint32_t SampleCount) {

	float * input;
	float * output;
	float freq;
	float depth;
	float gain;
	Tremolo * ptr;
	double sample_index;
	float phase = 0.0f;

	ptr = (Tremolo *)Instance;

	input = ptr->InputBuffer_1;
	output = ptr->OutputBuffer_1;
	freq = LIMIT(*(ptr->Control_Freq),0.0f,20.0f);
	depth = LIMIT(*(ptr->Control_Depth),0.0f,100.0f);
	
	// First some TAP handling
    if (*(ptr->Control_Tap) > 0) {
        float d = handle_tap(ptr);
        if (d > 0) 
            freq = (float) d; // ptr->tempo_tap = (float)d;
    }
	

	ptr->oldgain = (*(ptr->Control_Gain)+ptr->oldgain)*0.5;
	gain = db2lin(LIMIT(ptr->oldgain,-70.0f,20.0f));

  	for (sample_index = 0; sample_index < SampleCount; sample_index++) {
		phase = 1024.0f * freq * sample_index / ptr->SampleRate + ptr->Phase;

		while (phase >= 1024.0f)
			phase -= 1024.0f;

		*(output++) = *(input++) * gain *
			(1 - 0.5*depth/100 + 0.5 * depth/100 * cos_table[(unsigned long) phase]);
	}
	ptr->Phase = phase;
	while (ptr->Phase >= 1024.0f)
		ptr->Phase -= 1024.0f;
}

void
cleanup_Tremolo(LV2_Handle Instance) {
	free(Instance);
}

const void*
extension_data_Tremolo(const char* uri)
{
    return NULL;
}


static const
LV2_Descriptor Descriptor = {
    "http://moddevices.com/plugins/tap/tremolo",
    instantiate_Tremolo,
    connect_port_Tremolo,
    activate_Tremolo,
    run_Tremolo,
    deactivate_Tremolo,
    cleanup_Tremolo,
    extension_data_Tremolo
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor*
lv2_descriptor(uint32_t index)
{
    if (index == 0) return &Descriptor;
    else return NULL;

}
