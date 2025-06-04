/*
  Hatari - audio.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  This file contains the routines which pass the audio data to the SDL library,
  with audio files baked into the executable using xxd-generated C arrays.
*/

#include <SDL.h>
#include "main.h"
#include "audio.h"
#include "../m68000.h"

#include "sfx_data.h"
#include "music_data.h"

#ifdef OGG_MUSIC
#define OGG_IMPL
#define VORBIS_IMPL
#include "minivorbis.h"
#endif /* OGG_MUSIC */
BOOL bDisableSound = FALSE;

#define SND_FREQ 22050

/* Converted frontier SFX to wav samples. */
#define MAX_CHANNELS 4

typedef struct wav_stream
{
	Uint8 *buf;
	int buf_pos;
	int buf_len;
	int loop; /* -1 no loop, otherwise specifies loop start pos */
} wav_stream;

#define MAX_WAV_SAMPLES 8
#define MAX_SFX_SAMPLES 33

wav_stream sfx_buf[MAX_SFX_SAMPLES];
wav_stream wav_channels[MAX_CHANNELS];

BOOL bSoundWorking = TRUE;			  /* Is sound OK */
volatile BOOL bPlayingBuffer = FALSE; /* Is playing buffer? */
int SoundBufferSize = 1024;			  /* Size of sound buffer */

#ifdef OGG_MUSIC
static OggVorbis_File music_file;
static int music_mode;
static BOOL music_playing = FALSE;
static int enabled_tracks;

MusicData music_data[MAX_WAV_SAMPLES];
SfxData sfx_data[MAX_SFX_SAMPLES];

/* Callbacks for libvorbis to read OGG from memory */
static size_t rw_read(void *ptr, size_t size, size_t nmemb, void *datasource)
{
    SDL_RWops *rw = (SDL_RWops *)datasource;
    return SDL_RWread(rw, ptr, size, nmemb);
}

static int rw_seek(void *datasource, ogg_int64_t offset, int whence)
{
    SDL_RWops *rw = (SDL_RWops *)datasource;
    return SDL_RWseek(rw, offset, whence) >= 0 ? 0 : -1;
}

static int rw_close(void *datasource)
{
    SDL_RWops *rw = (SDL_RWops *)datasource;
    SDL_RWclose(rw);
    return 0;
}

static long rw_tell(void *datasource)
{
    SDL_RWops *rw = (SDL_RWops *)datasource;
    return SDL_RWtell(rw);
}

static ov_callbacks vorbis_callbacks = {
    rw_read,
    rw_seek,
    rw_close,
    rw_tell
};

static void play_music(int track)
{
    if (track < 0 || track >= (sizeof(music_data) / sizeof(music_data[0])))
    {
        music_playing = FALSE;
        return;
    }

    if (music_playing == TRUE)
        ov_clear(&music_file);

    SDL_RWops *rw = SDL_RWFromConstMem(music_data[track].data, music_data[track].len);
    if (rw == NULL)
    {
        fprintf(stderr, "Could not create RWops for music track %d: %s\n", track, SDL_GetError());
        music_playing = FALSE;
        return;
    }

    if (ov_open_callbacks(rw, &music_file, NULL, 0, vorbis_callbacks) < 0)
    {
        fprintf(stderr, "Libvorbis could not open music track %d\n", track);
        SDL_RWclose(rw);
        music_playing = FALSE;
        return;
    }

    music_playing = TRUE;
}

int rand_tracknum()
{
    int track;
    if (enabled_tracks == 0)
        return 999;
    do
    {
        track = rand() % (sizeof(music_data) / sizeof(music_data[0]));
    } while ((enabled_tracks & (1 << track)) == 0);
    return track;
}
#endif /* OGG_MUSIC */

void Call_PlayMusic()
{
#ifdef OGG_MUSIC
    /* Playing mode in d0:
     * -2 = play random track once
     * -1 = play random tracks continuously
     * 0+ = play specific track once
     * d1:d2 is a mask of enabled tracks
     */



    music_mode = GetReg(0);

    enabled_tracks = 0;

    if (GetReg(1) & 0xff000000)
        enabled_tracks |= 0x1;
    if (GetReg(1) & 0xff0000)
        enabled_tracks |= 0x2;
    if (GetReg(1) & 0xff00)
        enabled_tracks |= 0x4;
    if (GetReg(1) & 0xff)
        enabled_tracks |= 0x8;
    if (GetReg(2) & 0xff000000)
        enabled_tracks |= 0x10;
    if (GetReg(2) & 0xff0000)
        enabled_tracks |= 0x20;
    if (GetReg(2) & 0xff00)
        enabled_tracks |= 0x40;
    if (GetReg(2) & 0xff)
        enabled_tracks |= 0x80;

    SDL_LockAudio();
    switch (music_mode)
    {
    case -2:
        /* hyperspace and battle music --
         * don't play blue danube or reward music */
        enabled_tracks &= ~0x40;
        enabled_tracks &= ~0x80;
        play_music(rand_tracknum());
        break;
    case -1:
        /* any music */
        play_music(rand_tracknum());
        break;
    default:
        play_music(music_mode);
        break;
    }
    SDL_UnlockAudio();
#endif /* OGG_MUSIC */
}

#ifdef OGG_MUSIC
static void stop_music()
{
    music_playing = FALSE;
    ov_clear(&music_file);
}
#endif /* OGG_MUSIC */

void Call_StopMusic()
{
#ifdef OGG_MUSIC
    SDL_LockAudio();
    stop_music();
    SDL_UnlockAudio();
#endif /* OGG_MUSIC */
}

void Call_IsMusicPlaying()
{
#ifdef OGG_MUSIC
    SetReg(0, music_playing);
#else
    SetReg(0, 0);
#endif /* OGG_MUSIC */
}

void Call_PlaySFX()
{
    int sample, chan;

    SDL_LockAudio();

    sample = (short)GetReg(REG_D0);
    chan = (short)GetReg(REG_D1);

    wav_channels[chan].buf_pos = 0;
    wav_channels[chan].buf_len = sfx_buf[sample].buf_len;
    wav_channels[chan].buf = sfx_buf[sample].buf;
    wav_channels[chan].loop = sfx_buf[sample].loop;

    SDL_UnlockAudio();
}

/*-----------------------------------------------------------------------*/
/*
  SDL audio callback function - copy emulation sound to audio system.
*/
void Audio_CallBack(void *userdata, Uint8 *pDestBuffer, int len)
{
    Sint8 *pBuffer;
    int i, j;
    short sample;
    BOOL playing = FALSE;

    pBuffer = (Sint8 *)pDestBuffer;

    for (i = 0; i < MAX_CHANNELS; i++)
    {
        if (wav_channels[i].buf != NULL)
        {
            playing = TRUE;
            break;
        }
    }

    memset(pDestBuffer, 0, len);

#ifdef OGG_MUSIC
    if (music_playing)
    {
        i = 0;
        while (i < len)
        {
            int amt;
            int music_section;
            amt = ov_read(&music_file, (char *)&pDestBuffer[i],
                          (len - i), 0, 2, 1, &music_section);
            i += amt;

            /* end of stream */
            if (amt == 0)
            {
                if (music_mode == -1)
                {
                    play_music(rand_tracknum());
                }
                else
                {
                    stop_music();
                }
                break;
            }
        }
    }
#endif /* OGG_MUSIC */

    if (!playing)
        return;

    for (i = 0; i < len; i += 4)
    {
        sample = 0;
        for (j = 0; j < MAX_CHANNELS; j++)
        {
            if (wav_channels[j].buf == NULL)
                continue;
            sample += *(short *)(wav_channels[j].buf + wav_channels[j].buf_pos);
            wav_channels[j].buf_pos += 2;
            if (wav_channels[j].buf_pos >= wav_channels[j].buf_len)
            {
                /* end of sample. either loop or terminate */
                if (wav_channels[j].loop != -1)
                {
                    wav_channels[j].buf_pos = wav_channels[j].loop;
                }
                else
                {
                    wav_channels[j].buf = NULL;
                }
            }
        }
        /* stereo! */
        *((short *)pBuffer) += sample;
        pBuffer += 2;
        *((short *)pBuffer) += sample;
        pBuffer += 2;
    }
}

/*
 * Loaded samples must be SND_FREQ, 16-bit signed. Reject
 * other frequencies but convert 8-bit unsigned.
 */
void check_sample_format(SDL_AudioSpec *spec, Uint8 **buf, int *len, const char *filename)
{
    Uint8 *old_buf = *buf;
    short *new_buf;
    int i;

    if (spec->freq != SND_FREQ)
    {
        printf("Sample %s is the wrong sample rate (wanted %dHz). Ignoring.\n", filename, SND_FREQ);
        SDL_FreeWAV(*buf);
        *buf = NULL;
        return;
    }

    if (spec->format == AUDIO_U8)
    {
        new_buf = malloc((*len) * 2);
        for (i = 0; i < (*len); i++)
        {
            new_buf[i] = (old_buf[i] ^ 128) << 8;
        }
        *len *= 2;
        SDL_FreeWAV(old_buf);
        *buf = (Uint8 *)new_buf;
    }
    else if (spec->format != AUDIO_S16)
    {
        printf("Sample %s is not 16-bit-signed or 8-bit unsigned. Ignoring.\n", filename);
        SDL_FreeWAV(*buf);
        *buf = NULL;
        return;
    }
}

/*-----------------------------------------------------------------------*/
/*
  Initialize the audio subsystem. Return TRUE if all OK.
  We use direct access to the sound buffer, set to a signed 16-bit stereo stream.
*/
void Audio_Init(void)
{
    music_data[0] = (MusicData){ music_00_ogg, music_00_ogg_len };
    music_data[1] = (MusicData){ music_01_ogg, music_01_ogg_len };
    music_data[2] = (MusicData){ music_02_ogg, music_02_ogg_len };
    music_data[3] = (MusicData){ music_03_ogg, music_03_ogg_len };
    music_data[4] = (MusicData){ music_04_ogg, music_04_ogg_len };
    music_data[5] = (MusicData){ music_05_ogg, music_05_ogg_len };
    music_data[6] = (MusicData){ music_06_ogg, music_06_ogg_len };
    music_data[7] = (MusicData){ music_07_ogg, music_07_ogg_len };

    sfx_data[0]  = (SfxData){ sfx_00_wav, sfx_00_wav_len };
    sfx_data[1]  = (SfxData){ sfx_01_wav, sfx_01_wav_len };
    sfx_data[2]  = (SfxData){ sfx_02_wav, sfx_02_wav_len };
    sfx_data[3]  = (SfxData){ sfx_03_wav, sfx_03_wav_len };
    sfx_data[4]  = (SfxData){ sfx_04_wav, sfx_04_wav_len };
    sfx_data[5]  = (SfxData){ sfx_05_wav, sfx_05_wav_len };
    sfx_data[6]  = (SfxData){ sfx_06_wav, sfx_06_wav_len };
    sfx_data[7]  = (SfxData){ sfx_07_wav, sfx_07_wav_len };
    sfx_data[8]  = (SfxData){ sfx_08_wav, sfx_08_wav_len };
    sfx_data[9]  = (SfxData){ sfx_09_wav, sfx_09_wav_len };
    sfx_data[10] = (SfxData){ sfx_10_wav, sfx_10_wav_len };
    sfx_data[11] = (SfxData){ sfx_11_wav, sfx_11_wav_len };
    sfx_data[12] = (SfxData){ sfx_12_wav, sfx_12_wav_len };
    sfx_data[13] = (SfxData){ sfx_13_wav, sfx_13_wav_len };
    sfx_data[14] = (SfxData){ sfx_14_wav, sfx_14_wav_len };
    sfx_data[15] = (SfxData){ sfx_15_wav, sfx_15_wav_len };
    sfx_data[16] = (SfxData){ sfx_16_wav, sfx_16_wav_len };
    sfx_data[17] = (SfxData){ sfx_17_wav, sfx_17_wav_len };
    sfx_data[18] = (SfxData){ sfx_18_wav, sfx_18_wav_len };
    sfx_data[19] = (SfxData){ sfx_19_wav, sfx_19_wav_len };
    sfx_data[20] = (SfxData){ sfx_20_wav, sfx_20_wav_len };
    sfx_data[21] = (SfxData){ sfx_21_wav, sfx_21_wav_len };
    sfx_data[22] = (SfxData){ sfx_22_wav, sfx_22_wav_len };
    sfx_data[23] = (SfxData){ sfx_23_wav, sfx_23_wav_len };
    sfx_data[24] = (SfxData){ sfx_24_wav, sfx_24_wav_len };
    sfx_data[25] = (SfxData){ sfx_25_wav, sfx_25_wav_len };
    sfx_data[26] = (SfxData){ sfx_26_wav, sfx_26_wav_len };
    sfx_data[27] = (SfxData){ sfx_27_wav, sfx_27_wav_len };
    sfx_data[28] = (SfxData){ sfx_28_wav, sfx_28_wav_len };
    sfx_data[29] = (SfxData){ sfx_29_wav, sfx_29_wav_len };
    sfx_data[30] = (SfxData){ sfx_30_wav, sfx_30_wav_len };
    sfx_data[31] = (SfxData){ sfx_31_wav, sfx_31_wav_len };
    sfx_data[32] = (SfxData){ sfx_32_wav, sfx_32_wav_len };

    int i;
    SDL_AudioSpec desiredAudioSpec;

    if (bDisableSound)
    {
        printf("Sound: Disabled\n");
        bSoundWorking = FALSE;
        return;
    }

    if (SDL_WasInit(SDL_INIT_AUDIO) == 0)
    {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
        {
            fprintf(stderr, "Could not init audio: %s\n", SDL_GetError());
            bSoundWorking = FALSE;
            return;
        }
    }

    desiredAudioSpec.freq = SND_FREQ;
    desiredAudioSpec.format = AUDIO_S16;
    desiredAudioSpec.channels = 2;
    desiredAudioSpec.samples = 1024;
    desiredAudioSpec.callback = Audio_CallBack;
    desiredAudioSpec.userdata = NULL;

    if (SDL_OpenAudio(&desiredAudioSpec, NULL))
    {
        fprintf(stderr, "Can't use audio: %s\n", SDL_GetError());
        bSoundWorking = FALSE;
        return;
    }

    SoundBufferSize = desiredAudioSpec.size;

    for (i = 0; i < MAX_SFX_SAMPLES; i++)
    {
        SDL_RWops *rw = SDL_RWFromConstMem(sfx_data[i].data, sfx_data[i].len);
        if (rw == NULL)
        {
            printf("Error creating RWops for sfx_%02d: %s\n", i, SDL_GetError());
            sfx_buf[i].buf = NULL;
            continue;
        }

        if (SDL_LoadWAV_RW(rw, 1, &desiredAudioSpec, &sfx_buf[i].buf, (Uint32 *)&sfx_buf[i].buf_len) == NULL)
        {
            printf("Error loading sfx_%02d: %s\n", i, SDL_GetError());
            sfx_buf[i].buf = NULL;
        }
        else
        {
            check_sample_format(&desiredAudioSpec, &sfx_buf[i].buf, &sfx_buf[i].buf_len, "embedded sfx");
        }

        /* 19 (hyperspace) and 23 (noise) loop */
        if (i == 19)
            sfx_buf[i].loop = SND_FREQ;
        else if (i == 23)
            sfx_buf[i].loop = 0;
        else
            sfx_buf[i].loop = -1;
    }

    bSoundWorking = TRUE;
    Audio_EnableAudio(TRUE);
}

/*-----------------------------------------------------------------------*/
/*
  Free audio subsystem
*/
void Audio_UnInit(void)
{
    int i;
    Audio_EnableAudio(FALSE);

    for (i = 0; i < MAX_SFX_SAMPLES; i++)
    {
        if (sfx_buf[i].buf)
        {
            SDL_FreeWAV(sfx_buf[i].buf);
            sfx_buf[i].buf = NULL;
        }
    }

#ifdef OGG_MUSIC
    if (music_playing)
        ov_clear(&music_file);
#endif

    SDL_CloseAudio();
}

/*-----------------------------------------------------------------------*/
/*
  Start/Stop sound buffer
*/
void Audio_EnableAudio(BOOL bEnable)
{
    if (bEnable && !bPlayingBuffer)
    {
        SDL_PauseAudio(FALSE);
        bPlayingBuffer = TRUE;
    }
    else if (!bEnable && bPlayingBuffer)
    {
        SDL_PauseAudio(!bEnable);
        bPlayingBuffer = bEnable;
    }
}