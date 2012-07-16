/*******************************************************************************
 * @file pct.c
 * @date 
 * @brief 
 *
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <pcap.h>

int
main(int argc, char** argv)
{
    /* char *dev="eth0"; */
    /* char errbuf[PCAP_ERRBUF_SIZE]; */
    /* pcap_t *handle; */

    /* dev = pcap_lookupdev(errbuf); */
    /* if ( dev == NULL ) */
    /* { */
    /*     fprintf(stderr, "Couldn't find device: %s\n", errbuf); */
    /*         return 2; */
    /* } */
    /* printf ("Dev: [%s]\n",dev); */

    /* handle = pcap_open_live(dev, BUFSIZ, 1, 1000, errbuf); */

    /* if (handle == NULL) */
    /* { */
    /*     fprintf (stderr, "Could not open device %s: %s\n", dev, errbuf); */
    /*     return 2; */
    /* } */


    pcap_t *handle;                /* Session handle */
    char dev[] = "eth0";           /* Device to sniff on */
    char errbuf[PCAP_ERRBUF_SIZE];	/* Error string */
    struct bpf_program fp;         /* The compiled filter expression */
    char filter_exp[] = "port 80";	/* The filter expression */
    bpf_u_int32 mask;              /* The netmask of our sniffing device */
    bpf_u_int32 net;               /* The IP of our sniffing device */
    struct pcap_pkthdr header;
    const u_char *packet;
     
    // Properties
    if (pcap_lookupnet(dev, &net, &mask, errbuf) == -1) {
        fprintf(stderr, "Can't get netmask for device %s\n", dev);
        net = 0;
        mask = 0;
    }
     
    handle = pcap_open_live(dev, BUFSIZ, 1, 5000, errbuf);
    if (handle == NULL) {
        fprintf(stderr, "Couldn't open device %s: %s\n", dev, errbuf);
        return(2);
    }

    // Filter
    if (pcap_compile(handle, &fp, filter_exp, 0, net) == -1) {
        fprintf(stderr, "Couldn't parse filter %s: %s\n", filter_exp, pcap_geterr(handle));
        return(2);
    }
     
    // Apply
    if (pcap_setfilter(handle, &fp) == -1) {
        fprintf(stderr,
                "Couldn't install filter %s: %s\n", 
                filter_exp,
                pcap_geterr(handle));
        return(2);
    }

    // grab
    packet = pcap_next(handle , &header);
    // Print info
    printf ("Length: %d\n", header.len);

    // Clean
    pcap_close(handle);
    return 0;
}

