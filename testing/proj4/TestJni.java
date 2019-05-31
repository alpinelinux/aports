import org.proj4.*;
import java.util.Arrays;

/**
 * Converts coordinates from EPSG:32632 (WGS 84 / UTM zone 32N) to WGS84,
 * then prints the result to the standard output stream.
 */
public class TestJni {
    public static void main(String[] args) throws PJException {
        PJ sourcePJ = new PJ("+init=epsg:32632");           // (x,y) axis order
        PJ targetPJ = new PJ("+proj=latlong +datum=WGS84"); // (λ,φ) axis order
        double[] coordinates = {
                500000,       0,   // First coordinate
                400000,  100000,   // Second coordinate
                600000, -100000    // Third coordinate
        };
        sourcePJ.transform(targetPJ, 2, coordinates, 0, 3);
        System.out.println(Arrays.toString(coordinates));
    }
}
