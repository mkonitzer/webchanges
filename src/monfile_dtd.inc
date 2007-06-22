const char monfile_dtd[] = \
    "<!ELEMENT monitorfile (document*)>\n" \
    "<!ATTLIST monitorfile name CDATA #REQUIRED>\n" \
    "\n" \
    "<!ELEMENT document (monitor*)>\n" \
    "<!ATTLIST document url CDATA #REQUIRED>\n" \
    "\n" \
    "<!ELEMENT monitor (xpath,trigger?,interval?)>\n" \
    "<!ATTLIST monitor name CDATA #REQUIRED>\n" \
    "<!ELEMENT xpath (#PCDATA)>\n" \
    "<!ELEMENT trigger (#PCDATA)>\n" \
    "<!ELEMENT interval (#PCDATA)>\n" \
;
