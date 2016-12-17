import java.io.*;
import java.net.*;

public class FileClient {

    private static final int MAX_BUF = 100;

    /*
    request a file from the server
    usage: java FileClient hostname portnumber secretkey filename configfile.dat
     */
    public static void main(String args[]) {

        // check if the correct number of arguments have been passed
        if (args.length < 5) {
            System.out.println("Format is 'java FileClient hostname portnumber secretkey filename configfile.dat'");
            System.exit(1);
        }

        String hostName = args[0];
        int port = Integer.parseInt(args[1]);
        String secretKey = args[2];
        String fileName = args[3];
        String configFileName = args[4];

         InetAddress address = null;
         try{
             address = InetAddress.getByName(hostName);
         }
         catch(UnknownHostException e){
             System.out.println("Cannot find hostname to ip mapping");
             e.printStackTrace();
             System.exit(1);
         }

        // check if filename is valid
        if (fileName.contains("/") || fileName.length() > 16) {
            System.out.println("Filename " + fileName + " is invalid");
            System.exit(1);
        }

        // if file already exists, exit
        File outFile = new File("./" + fileName);
        if (outFile.exists() && !outFile.isDirectory()) {
            System.out.println("File " + fileName + " already exists.");
            System.exit(1);
        }

        // open the config file for reading
        FileInputStream configFileStream = null;
        try {
            configFileStream = new FileInputStream(configFileName);
        } catch (FileNotFoundException e) {
            System.out.println("Could not find file " + configFileName);
            e.printStackTrace();
            System.exit(1);
        }

        // get number of bytes to be read from the config file
        byte[] blockSizeBytes = new byte[MAX_BUF];
        int bytesRead = 0;
        try {
            bytesRead = configFileStream.read(blockSizeBytes);
            configFileStream.close();
        } catch (IOException e) {
            System.out.println("IOException while reading config file " + configFileName);
            e.printStackTrace();
            System.exit(1);
        }

        // check if anything was read
        if (bytesRead <= 0) {
            System.out.println("There was no data in the config file " + configFileName);
            System.exit(1);
        }

        // convert the read bytes into the block length value
        int blockLength = 0;
        for (int i = 0; i < bytesRead - 1; ++i) {
            blockLength = blockLength * 10 + (blockSizeBytes[i] - '0');
        }

        // construct the client request
        byte[] clientRequest = ("$" + secretKey + "$" + fileName).getBytes();

        // create and open the file for writing
        FileOutputStream fileOutputStream = null;
        try {
            fileOutputStream = new FileOutputStream(fileName);
        } catch (FileNotFoundException e) {
            System.out.println("File " + fileName + " is a directory, cannot be created or cannot be opened for some reason");
            e.printStackTrace();
            System.exit(1);
        }

        // readbuffer of appropriate length
        byte[] readBuffer = new byte[blockLength + 1];
        boolean firstReadDone = false;

        long startTime = 0;
        int totalBytesRead = 0;
        try {
            // open socket to server
            Socket socket = new Socket(address.getHostAddress(), port);

            // send the client request
            OutputStream outputStream = socket.getOutputStream();
            outputStream.write(clientRequest);

            // inputstream to read from socket
            InputStream inputStream = socket.getInputStream();
            startTime = 0;
            int bytes;
            while ((bytes = inputStream.read(readBuffer)) > 0) {
                if (!firstReadDone) {
                    firstReadDone = true;
                    startTime = System.nanoTime();
                }

                // write all the bytes that are read into the file
                fileOutputStream.write(readBuffer, 0, bytes);

                totalBytesRead += bytes;
            }
            fileOutputStream.close();
        } catch (IOException e) {
            e.printStackTrace();
        }

        long completionTime = System.nanoTime() - startTime;
        System.out.println("Completion time: " + completionTime/1000000.0 + " msec");
        float reliableThroughput = (float) ((8*totalBytesRead) / (completionTime / 1000000000.0));
        System.out.println("Total bytes read: " + totalBytesRead);
        System.out.println("Reliable throughput: " + reliableThroughput + " bits per second");
    }
}
