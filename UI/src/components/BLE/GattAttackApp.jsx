import { useState, useEffect } from 'react';

import Grid from '@mui/material/Grid';
import Button from '@mui/material/Button';
import Typography from '@mui/material/Typography';
import FormGroup from '@mui/material/FormGroup';
import FormControlLabel from '@mui/material/FormControlLabel';
import Switch from '@mui/material/Switch';
import Box from '@mui/material/Box';

import TextField from '@mui/material/TextField';
import { MitmPanel } from './MitmPanel';
import { HookEditor } from './HookEditor';

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
  const [manualMac, setManualMac] = useState('');

  const stop = () => {
    if (window.socket?.readyState === 1) {
      if (confirm('r u sure?')) {
        setTarget(false);
        setResults([]);
        window.socket.send(JSON.stringify({
          opt: app.opt,
          action: 'stop'
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
          action: 'start',
          id: dev.id,
        }));
      }
    }
  };

  const startManual = () => {
    if (window.socket?.readyState === 1) {
      if (confirm(`Connect to ${manualMac}?`)) {
        setTarget({ mac: manualMac, name: 'Manual Device' });
        window.socket.send(JSON.stringify({
          opt: app.opt,
          action: 'start',
          mac: manualMac,
        }));
      }
    }
  };

  const scanDevices = () => {
    if (window.socket?.readyState === 1) {
      window.socket.send(JSON.stringify({
        opt: app.opt,
        action: 'scan',
      }));
    }
  };

  const toggleHookState = (e) => {
    window.socket.send(JSON.stringify({
      opt: 2, // or app.opt
      action: 'enable_hooks',
      enable: e.target.checked
    }));
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
          action: 'results',
        }));
      }
    }

    if (data.scan_done) {
      window.socket.send(JSON.stringify({
        opt: app.opt,
        action: 'results',
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
        action: 'hookret',
        hook_ret,
      }));
    }

    if (data.onWrite) {
      const hook_ret = window.hook_write(data);
      window.socket.send(JSON.stringify({
        opt: app.opt,
        action: 'hookret',
        hook_ret,
      }));
    }
  }

  const setup = () => {
    try {
      if (window.socket?.readyState === 1) {
        window.socket.send(JSON.stringify({
          opt: app.opt,
          action: 'status',
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
        <Box sx={{ display: 'flex', flexDirection: 'column', gap: 2, maxWidth: 400 }}>
          <Button
            type='submit'
            variant='contained'
            color='primary'
            onClick={scanDevices}
          >
            Scan
          </Button>

          <Typography variant='body1' sx={{ textAlign: 'center' }}>OR</Typography>

          <Box sx={{ display: 'flex', gap: 1 }}>
            <TextField
              label='MAC Address'
              variant='outlined'
              size='small'
              value={manualMac}
              onChange={(e) => setManualMac(e.target.value)}
              placeholder='AA:BB:CC:DD:EE:FF'
              fullWidth
            />
            <Button
              variant='contained'
              color='primary'
              onClick={startManual}
              disabled={manualMac.length < 17}
            >
              Connect
            </Button>
          </Box>
        </Box>
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

          <Box sx={{ mb: 3 }}>
            <FormGroup>
              <FormControlLabel
                control={
                  <Switch
                    checked={hooksEnabled}
                    onChange={toggleHookState}
                  />
                }
                label='Enable Hooks'
              />
            </FormGroup>
            {hooksEnabled && (
              <Grid container spacing={2}>
                <Grid size={{ xs: 12, md: 6 }}>
                  <Typography variant='h6'>Read Hook</Typography>
                  <HookEditor />
                </Grid>
                <Grid size={{ xs: 12, md: 6 }}>
                  <Typography variant='h6'>Write Hook</Typography>
                  <HookEditor hook='write' />
                </Grid>
              </Grid>
            )}
          </Box>

          <Grid container spacing={3}>
            {results.sort((a, b) => b.rssi - a.rssi).map((r) => (
              <Grid size={{ xs: 12, sm: 6, md: 4, lg: 3 }} key={r.id}>
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
