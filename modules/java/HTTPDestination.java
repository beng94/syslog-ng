/**
 * Created on 6/29/15.
 */
import org.syslog_ng.*;

import java.io.*;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;


public class HTTPDestination extends TextLogDestination {

    private String name;
    private URL url;

    public HTTPDestination(long arg0) {
        super(arg0);
    }

    public void deinit() {
    }

    public void onMessageQueueEmpty() {
    }

    public boolean init() {
        this.name = getOption("name");
        try {
            this.url = new URL(getOption("url"));
        } catch (MalformedURLException e) {
            InternalMessageSender.error("A properly formatted URL is a required option for this destination");
            return false;
        }
        if (name == null) {
            InternalMessageSender.error("A unique name is a required option for this destination");
            return false;
        }

        InternalMessageSender.debug("Init " + name);
        return true;
    }

    public boolean open() {
        return true;
    }

    public boolean isOpened() {
        return true;
    }

    public void close() {

    }

    public boolean send(String message) {
        int responseCode=0;
        try {
            HttpURLConnection connection = (HttpURLConnection) this.url.openConnection();
            connection.setRequestMethod("PUT");
            connection.setRequestProperty( "content-length", Integer.toString(message.length()) );
            connection.setDoOutput(true);
            OutputStreamWriter osw = new OutputStreamWriter(connection.getOutputStream());
            osw.write(message);
            osw.flush();
            osw.close();
            responseCode=connection.getResponseCode();
        }
        catch (Exception e)
        {

            InternalMessageSender.error("error in writing message." +
                    (responseCode!=0 ? "Response code is "+responseCode : "") +" . Name: " + name);
            return false;
        }
        return true;
    }
}

