import { useState } from 'react';

import Button from '@mui/material/Button';
import TextField from '@mui/material/TextField';
import Typography from '@mui/material/Typography';

const Reboot = () => {
  const reboot = (e) => {
    e.preventDefault();
    if (confirm('are u sure?')) {
      window.socket.send(JSON.stringify({
        opt: 1,
        value: 9001,
      }));
    }
  };

  return (
    <>
      <Typography variant='h4' sx={{ mt: 3 }}>Reboot</Typography>
      
      <form onSubmit={reboot}>
        <Button
          type='submit'
          variant='contained'
          color='primary'
          onClick={reboot}
        >
          Reboot
        </Button>
      </form>
    </>
  );
};

const Wifi = () => {
  const [ssid, setSSID] = useState('');
  const [pass, setPass] = useState('');

  const updateWifi = (e) => {
    e.preventDefault();
    window.socket.send(JSON.stringify({
      opt: 0,
      ssid,
      pass
    }));
  };

  return (
    <>
      <Typography variant='h4' sx={{ mt: 3 }}>Set Wifi</Typography>
      
      <Typography variant='body1' sx={{ mb: 3 }}>
        Set the Wifi SSID/Pass for the device. Requires restart!
      </Typography>
      
      <form onSubmit={updateWifi}>
        <TextField
          value={ssid}
          onChange={(e) => setSSID(e.target.value)}
          size='small'
          placeholder='Daves iPhone 17'
        />
        <TextField
          sx={{ ml: 2 }}
          value={pass}
          onChange={(e) => setPass(e.target.value)}
          size='small'
          placeholder='executive1'
        />
        <Button
          type='submit'
          sx={{ ml: 2 }}
          variant='contained'
          color='primary'
          onClick={updateWifi}
        >
          Set
        </Button>
      </form>
    </>
  );
};

export const Settings = () => {
  return (
    <>
      <Typography variant='h3'>Settings</Typography>
      
      <hr />

      <Wifi />

      <Reboot />
    </>
  );
};

export default Settings;
