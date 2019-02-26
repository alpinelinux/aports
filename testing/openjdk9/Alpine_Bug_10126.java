public class Alpine_Bug_10126 {
    public static void main(String[] args) throws Exception {
        try (java.net.Socket sock = javax.net.ssl.SSLSocketFactory.getDefault().createSocket("bugs.alpinelinux.org", 443);
             java.io.InputStream in = sock.getInputStream();
             java.io.OutputStream out = sock.getOutputStream()) {
            out.write("GET / HTTP/1.0\n\nHost: bugs.alpinelinux.org\n\nConnection: close\n\n\n\n".getBytes());
            out.flush();
            while (in.read(new byte[1024]) != -1) ;
        }
        System.out.println("Secured connection performed successfully");
    }
}

