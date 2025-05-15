struct envia
{
    char dominio[100] ;
};

struct retorno
{
    char retips[100] ;
};

program IPS_PROGRAMA{
    version VERSION_IPS_PROGRAMA{
        struct retorno ips (struct envia) = 1;
   }  =  1;
} = 0x20000001;
