struct envia {
	char nombre[100];
};

struct retornoDireccion {
	int disponible;
	char direccion[15];
};

program SNA_SOLICITAR_DIRECCION_PROGRAMA {
	version VERSION_SNA_SOLICITAR_PROGRAMA {
		struct retornoDireccion sna_solicitar (struct envia) = 1;
	} = 1;
} = 0x20000001;