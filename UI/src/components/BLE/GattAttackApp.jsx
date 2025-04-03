import { useState, useEffect } from 'react';

import Grid from '@mui/material/Grid';
import Button from '@mui/material/Button';
import Typography from '@mui/material/Typography';

import { MitmPanel } from './MitmPanel';

import { Device, CancelButton } from '../common';

const app = {
  opt: 2,
};

function parseGattOperation(m) {
  const { operation, direction, type, trans_id, handle, uuid, data } = m;
  const typeStr = type ? `${type}` : '';
  const dataStr = data ? ` | Data: ${data}` : '';
  return `[${operation}] ${typeStr} | ${direction} | TX_ID: ${trans_id} | Handle: ${handle} | UUID: ${uuid}${dataStr}`;
}

export const GattAttackApp = () => {
  const [state, setState] = useState(-1);
  const [hooksEnabled, setHooksEnabled] = useState(false);
  const [target, setTarget] = useState(false);
  const [messages, setMessages] = useState([]);
  const [results, setResults] = useState([]);

  const stop = () => {
    if (window.socket?.readyState === 1) {
      if (confirm('r u sure?')) {
        setTarget(false);
        setResults([]);
        window.socket.send(JSON.stringify({
          opt: app.opt,
          action: "stop"
        }));
      }
    }
  };

  const start = (dev) => {
    if (window.socket?.readyState === 1) {
      if (confirm('r u sure?')) {
        setTarget(dev);
        window.socket.send(JSON.stringify({
          opt: app.opt,
          action: "start",
          id: dev.id,
        }));
      }
    }
  };

  const scanDevices = () => {
    if (window.socket?.readyState === 1) {
      window.socket.send(JSON.stringify({
        opt: app.opt,
        action: "scan",
      }));
    }
  };

  window.socketCallback = (data) => {
    if (data.connected) {
      setup();
    }

    if ('hooks_enabled' in data) {
      setHooksEnabled(data.hooks_enabled);
    }

    if (data.state > -1) {
      setState(data.state);

      // get ble device results
      if (data.state === 2 && results.length < 1) {
        window.socket.send(JSON.stringify({
          opt: app.opt,
          action: "results",
        }));
      }
    }

    if (data.scan_done) {
      window.socket.send(JSON.stringify({
        opt: app.opt,
        action: "results",
      }));
    }

    if (data.results) {
      setResults(data.results);
    }

    if (data.msg) {
      setMessages(prev => [...prev, data]);
    }

    if (data.operation) {
      setMessages(prev => [
        ...prev,
        { msg: parseGattOperation(data), ...data }
      ]);
    }

    if (data.onRead) {
      const hook_ret = window.hook_read(data);
      window.socket.send(JSON.stringify({
        opt: app.opt,
        action: "hookret",
        hook_ret,
      }));
    }

    if (data.onWrite) {
      const hook_ret = window.hook_write(data);
      window.socket.send(JSON.stringify({
        opt: app.opt,
        action: "hookret",
        hook_ret,
      }));
    }
  }

  const setup = () => {
    try {
      if (window.socket?.readyState === 1) {
        window.socket.send(JSON.stringify({
          opt: app.opt,
          action: "status",
        }));
      } else {
        setTimeout(setup, 2000);
      }
    } catch (err) {
      console.error(err);
    }
  }

  useEffect(() => {
    setup();
  }, []);

  return (
    <>
      {state === 0 && (
        <Button
          type='submit'
          variant='contained'
          color='primary'
          onClick={scanDevices}
        >
          Scan
        </Button>
      )}

      {state === 1 && (
        <Typography variant='body1'>
          scanning for devices...
        </Typography>
      )}

      {state === 2 && results.length > 0 && (
        <>
          <CancelButton onStop={stop} />

          <Typography variant='body1' sx={{ mb: 2 }}>
            Devices: {results.length}
          </Typography>

          <Grid container spacing={3}>
            {results.sort((a, b) => b.rssi - a.rssi).map((r) => (
              <Grid item xs={12} sm={6} md={4} lg={3} key={r.id}>
                <Device
                  dev={r}
                  onClick={() => start(r)}
                />
              </Grid>
            ))}
          </Grid>
        </>
      )}

      {state >= 3 &&
        <>
          <CancelButton onStop={stop} />
          <MitmPanel
            target={target}
            messages={messages}
            setMessages={setMessages}
            hooksEnabled={hooksEnabled}
          />
        </>
      }
    </>
  );
};

GattAttackApp.propTypes = {};

export default GattAttackApp;
