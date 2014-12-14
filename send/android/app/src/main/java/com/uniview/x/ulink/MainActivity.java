package com.uniview.x.ulink;

import android.os.Bundle;
import android.os.Message;
import android.support.v7.app.ActionBarActivity;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;


public class MainActivity extends ActionBarActivity {

    private Sender snd = null;


    private android.os.Handler handler = new android.os.Handler() {
        @Override
        public void handleMessage(Message msg) {
            Button btn;
            switch (msg.what) {
                case Sender.MSG_START:
                    btn = (Button) findViewById(R.id.btnTest);
                    btn.setText("Stop");
                    btn.setEnabled(true);
                    break;
                case Sender.MSG_STOP:
                    btn = (Button) findViewById(R.id.btnTest);
                    btn.setText("Send");
                    btn.setEnabled(true);
                    break;
            }
            super.handleMessage(msg);
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
    }


    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
        getMenuInflater().inflate(R.menu.menu_main, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle action bar item clicks here. The action bar will
        // automatically handle clicks on the Home/Up button, so long
        // as you specify a parent activity in AndroidManifest.xml.
        int id = item.getItemId();

        //noinspection SimplifiableIfStatement
        if (id == R.id.action_settings) {
            return true;
        }

        return super.onOptionsItemSelected(item);
    }

    public void onTest(View v) {
        Button btnSend = (Button) v;
        EditText txt = (EditText) findViewById(R.id.textToSend);
        btnSend.setEnabled(false);

        if (snd != null && snd.isRunning()) {
            snd.stop();
            snd = null;
        } else {
            byte[] bytes = txt.getText().toString().getBytes();
            byte[] bytesSend = new byte[bytes.length + 1];
            System.arraycopy(bytes, 0, bytesSend, 0, bytes.length);
            bytesSend[bytes.length] = 0;

            snd = new Sender();
            snd.setHandler(handler);
            try {
                snd.send(bytesSend);
            } catch (Exception ex) {
                ex.printStackTrace();
            }
        }
    }
}
