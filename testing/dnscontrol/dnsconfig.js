var namecom = NewRegistrar("name.com", "NAMEDOTCOM");
var r53 = NewDnsProvider("r53", "ROUTE53")

D("example.com", namecom, DnsProvider(r53),
    A("@", "1.2.3.4"),
    CNAME("www","@"),
    MX("@",5,"mail.myserver.com."),
    A("test", "5.6.7.8")
)
