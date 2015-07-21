# libpagemap
libpagemap project is aimed to provide simple access to pagemap interface of the Linux kernel

It consists of core library and some user-utilities (so far, only *pgmap* and *pagemapvisual*)

Library functions brings a lot of virtual memory statistics about every process in system.
User can get e.g. *RES*,*SHR*,*SWAP*, *USS* or *PSS* measurements as well as detailed
information about physical pages flags on which is process mapped. 

## Sources

* [git repository](https://github.com/pholasek/libpagemap/)
* [What is pagemap interface?](http://lwn.net/Articles/230975/)
* [pagemapvisual doc](http://pholasek.fedorapeople.org/pagemapvisual-doc/)
* [kernel doc](https://www.kernel.org/doc/Documentation/vm/pagemap.txt)
* [pagemap demo scripts](https://github.com/bzolnier/pagemap-demo-ng)
