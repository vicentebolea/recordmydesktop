/*********************************************************************************
*                             recordMyDesktop                                    *
**********************************************************************************
*                                                                                *
*             Copyright (C) 2006  John Varouhakis                                *
*                                                                                *
*                                                                                *
*    This program is free software; you can redistribute it and/or modify        *
*    it under the terms of the GNU General Public License as published by        *
*    the Free Software Foundation; either version 2 of the License, or           *
*    (at your option) any later version.                                         *
*                                                                                *
*    This program is distributed in the hope that it will be useful,             *
*    but WITHOUT ANY WARRANTY; without even the implied warranty of              *
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the               *
*    GNU General Public License for more details.                                *
*                                                                                *
*    You should have received a copy of the GNU General Public License           *
*    along with this program; if not, write to the Free Software                 *
*    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA   *
*                                                                                *
*                                                                                *
*                                                                                *
*    For further information contact me at johnvarouhakis@gmail.com              *
**********************************************************************************/

#include <recordmydesktop.h>

void *EncodeImageBuffer(void *pdata){
    pthread_mutex_t pmut,imut;
    pthread_mutex_init(&pmut,NULL);
    pthread_mutex_init(&imut,NULL);

    while(((ProgData *)pdata)->running){
        pthread_cond_wait(&((ProgData *)pdata)->image_buffer_ready,&imut);
        if(Paused)
            pthread_cond_wait(&((ProgData *)pdata)->pause_cond,&pmut);//this may not be needed
        pthread_mutex_lock(&((ProgData *)pdata)->yuv_mutex);
        int za=theora_encode_YUVin(&((ProgData *)pdata)->enc_data->m_th_st,&((ProgData *)pdata)->enc_data->yuv);
        if(za){
            fprintf(stderr,"Encoder not ready %d!\n",za);
        }
        pthread_mutex_unlock(&((ProgData *)pdata)->yuv_mutex);
        theora_encode_packetout(&((ProgData *)pdata)->enc_data->m_th_st,0,&((ProgData *)pdata)->enc_data->m_ogg_pckt1);
        ogg_stream_packetin(&((ProgData *)pdata)->enc_data->m_ogg_ts,&((ProgData *)pdata)->enc_data->m_ogg_pckt1);
        ((ProgData *)pdata)->avd+=((ProgData *)pdata)->frametime*2*((ProgData *)pdata)->args.channels;
    }
    //last packet
    if(theora_encode_YUVin(&((ProgData *)pdata)->enc_data->m_th_st,&((ProgData *)pdata)->enc_data->yuv)){
            fprintf(stderr,"Encoder not ready!\n");
    }

    theora_encode_packetout(&((ProgData *)pdata)->enc_data->m_th_st,1,&((ProgData *)pdata)->enc_data->m_ogg_pckt1);

//     ogg_stream_packetin(&((ProgData *)pdata)->enc_data->m_ogg_ts,&((ProgData *)pdata)->enc_data->m_ogg_pckt);

    pthread_exit(&errno);
}

