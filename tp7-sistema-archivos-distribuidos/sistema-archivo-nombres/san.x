struct envia
{
    char nombre[100] ;
};

struct retorno
{
    char direccion[100] ;
};

program SAN_PROGRAMA{
    version VERSION_SAN_PROGRAMA{
        struct retorno san (struct envia) = 1;
   }  =  1;
} = 0x20000001;
