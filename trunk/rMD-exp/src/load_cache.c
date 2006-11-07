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

void LoadBlock(unsigned char *dest,unsigned char *source,int blockno,int width, int height,int divisor){
    int j,
        block_i=blockno/divisor,//place on the grid
        block_k=blockno%divisor;

    for(j=0;j<height/divisor;j++)//we copy rows
        memcpy( &dest[block_i*(width*height/divisor)+j*width+block_k*width/divisor],
                &source[j*width/divisor],
                width/divisor);
}



void *LoadCache(void *pdata){

    yuv_buffer *yuv=&((ProgData *)pdata)->enc_data->yuv;
    gzFile *ifp=((ProgData *)pdata)->cache_data->ifp;
    FILE *afp=((ProgData *)pdata)->cache_data->afp;
    FrameHeader fheader;
    CachedFrame frame;
    signed char *sound_data=(signed char *)malloc(((ProgData *)pdata)->periodsize);

    int j=0,
        audio_end=0,
        extra_frames=0,//total number of duplicated frames
        missing_frames=0,//if this is found >0 current run will not load
                        //a frame but it will proccess the previous
        thread_exit=0,//0 success, -1 couldn't find files,1 couldn't remove
        divisor=16,
        blockszy=0,//size of y plane block in bytes
        blockszuv=0;//size of u,v plane blocks in bytes
    //we allocate the frame that we will use
    INIT_FRAME(&frame,&fheader,yuv)
    //and the we open our files
    ifp=gzopen(((ProgData *)pdata)->cache_data->imgdata,"rb");
    if(ifp==NULL){
        thread_exit=-1;
        pthread_exit(&thread_exit);
    }

    if(!((ProgData *)pdata)->args.nosound){
        afp=fopen(((ProgData *)pdata)->cache_data->audiodata,"rb");
        if(afp==NULL){
            thread_exit=-1;
            pthread_exit(&thread_exit);
        }
    }
    //these two are likely to be the same, but not guaranteed, especially on
    //low resolutions
    blockszy=(yuv->y_width*yuv->y_height )/pow(divisor,2);
    blockszuv=(yuv->uv_width*yuv->uv_height)/pow(divisor/2,2);

    //this will be used now to define if we proccess audio or video
    //on any given loop.
    ((ProgData *)pdata)->avd=0;
    //If sound finishes first,we go on with the video.
    //If video ends we will do one more run to flush audio in the ogg file

    while(((ProgData *)pdata)->running){
        //video load and encoding
        if(((ProgData *)pdata)->avd<=0 || ((ProgData *)pdata)->args.nosound || audio_end){
            if(missing_frames>0){
                extra_frames++;
                missing_frames--;
                SyncEncodeImageBuffer((ProgData *)pdata);
            }
            else if(gzread(ifp,frame.header,sizeof(FrameHeader))==sizeof(FrameHeader)){
                //sync
                missing_frames+=frame.header->current_total-(extra_frames+frame.header->frameno);
                fprintf(stdout,"\r[%d%%] ",
                ((frame.header->frameno+extra_frames)*100)/frames_total);

                fflush(stdout);
                if( (frame.header->Ynum<=pow(divisor,2)) &&
                    (frame.header->Unum<=pow(divisor/2,2)) &&
                    (frame.header->Vnum<=pow(divisor/2,2)) &&
                    (gzread(ifp,frame.YBlocks,frame.header->Ynum)==frame.header->Ynum) &&
                    (gzread(ifp,frame.UBlocks,frame.header->Unum)==frame.header->Unum) &&
                    (gzread(ifp,frame.VBlocks,frame.header->Vnum)==frame.header->Vnum) &&
                    (gzread(ifp,frame.YData,blockszy*frame.header->Ynum)==blockszy*frame.header->Ynum) &&
                    (gzread(ifp,frame.UData,(blockszuv*frame.header->Unum))==(blockszuv*frame.header->Unum)) &&
                    (gzread(ifp,frame.VData,(blockszuv*frame.header->Vnum))==(blockszuv*frame.header->Vnum))){
                        //load the blocks for each buffer
                        if(frame.header->Ynum)
                            for(j=0;j<frame.header->Ynum;j++)
                                LoadBlock(  yuv->y,
                                            &frame.YData[j*blockszy],
                                            frame.YBlocks[j],
                                            yuv->y_width,
                                            yuv->y_height,
                                            divisor);
                        if(frame.header->Unum)
                            for(j=0;j<frame.header->Unum;j++)
                                LoadBlock(  yuv->u,
                                            &frame.UData[j*blockszuv],
                                            frame.UBlocks[j],
                                            yuv->uv_width,
                                            yuv->uv_height,
                                            divisor/2);
                        if(frame.header->Vnum)
                            for(j=0;j<frame.header->Vnum;j++)
                                LoadBlock(  yuv->v,
                                            &frame.VData[j*blockszuv],
                                            frame.VBlocks[j],
                                            yuv->uv_width,
                                            yuv->uv_height,
                                            divisor/2);
                        //encode. This is not made in a thread since now blocking is not a problem
                        //and this way sync problems can be avoided more easily.
                        SyncEncodeImageBuffer((ProgData *)pdata);
                }
                else{
                    raise(SIGINT);
                    continue;
                }
            }
            else{
                raise(SIGINT);
                continue;
            }
        }
        //audio load and encoding
        else{
            if(!audio_end){
                int nbytes=fread(sound_data,((ProgData *)pdata)->periodsize,1,afp);
                if(nbytes<=0)
                    audio_end=1;
                else
                    SyncEncodeSoundBuffer((ProgData *)pdata,sound_data);
            }
        }
    }
    fprintf(stdout,"\n");
    CLEAR_FRAME(&frame)
    free(sound_data);
    gzclose(ifp);

    if(remove(((ProgData *)pdata)->cache_data->imgdata)){
        fprintf(stderr,"Couldn't remove temporary file %s",((ProgData *)pdata)->cache_data->imgdata);
        thread_exit=1;
    }
    if(!((ProgData *)pdata)->args.nosound){
        fclose(afp);
        if(remove(((ProgData *)pdata)->cache_data->audiodata)){
            fprintf(stderr,"Couldn't remove temporary file %s",((ProgData *)pdata)->cache_data->audiodata);
            thread_exit=1;
        }
    }
    if(remove(((ProgData *)pdata)->cache_data->projname)){
        fprintf(stderr,"Couldn't remove temporary directory %s",((ProgData *)pdata)->cache_data->projname);
        thread_exit=1;
    }

    pthread_exit(&thread_exit);


}
