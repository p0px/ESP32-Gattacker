import { useState, memo } from 'react';

import Grid from '@mui/material/Grid';
import Typography from '@mui/material/Typography';
import Card from '@mui/material/Card';
import CardContent from '@mui/material/CardContent';
import FormGroup from '@mui/material/FormGroup';
import FormControlLabel from '@mui/material/FormControlLabel';
import Switch from '@mui/material/Switch';
import Button from '@mui/material/Button';
import Box from '@mui/material/Box';

function copyToClipboard(textToCopy) {
  if (navigator.clipboard && window.isSecureContext) {
    navigator.clipboard.writeText(textToCopy);
  } else {
    const textarea = document.createElement('textarea');
    textarea.value = textToCopy;
    textarea.style.position = 'absolute';
    textarea.style.left = '-99999999px';
    document.body.prepend(textarea);
    textarea.select();

    try {
      document.execCommand('copy');
    } catch (err) {
      console.log(err);
    } finally {
      textarea.remove();
    }
  }
}

const defaultRead = `
(function hookRead({ uuid, value }) {
  // your code here
  return value;
})
`;

const defaultWrite = `
(function hookWrite({ uuid, value }) {
  // your code here
  return value;
})
`;

const testHook = (i) => {
  if (!/^[0-9a-fA-F]+$/.test(i) || i.length % 2 !== 0) {
    throw new Error('Output is not a valid hex-encoded string');
  }
  return true;
};

const toggleHookState = (e) => {
  window.socket.send(JSON.stringify({
    opt: 2,
    action: 'enable_hooks',
    enable: e.target.checked
  }));
};

window['hook_read'] = eval(defaultRead);
window['hook_write'] = eval(defaultWrite);

const HookEditor = ({
  hook = 'read'
}) => {
  const [error, setError] = useState(false);
  const [code, setCode] = useState(window[`hook_${hook}`].toString());
  
  const handleChange = (e) => {
    setCode(e.target.value);
    try {
      const test = eval(`(${e.target.value})`)({ uuid: 'test', 'value': '01020304'});
      if (testHook(test)) {
        window[`hook_${hook}`] = eval(`(${e.target.value})`);
      }
      
      setError(false);
    } catch (err) {
      setError(`Invalid function ${err}`);
    }
  };

  return (
    <Card>
      <CardContent sx={{ p: 2 }}>
        {error && <Typography>{error}</Typography>}
        <div style={{ display: 'grid', gap: '1rem' }}>
          <textarea 
            value={code}
            onChange={handleChange}
            style={{
              backgroundColor: '#0f172a',
              color: '#f8fafc',
              padding: '1rem',
              borderRadius: '0.375rem',
              minHeight: '200px',
              width: '100%'
            }}
          />
        </div>
      </CardContent>
    </Card>
  );
};

export const MitmPanel = memo(({
  target,
  messages,
  setMessages,
  hooksEnabled = false,
}) => (
  <Grid container spacing={2}>
    <Grid item xs={12} lg={8}>
      <Box display='flex' alignItems='center' gap={1}>
        <Typography variant='h6'>Messages</Typography>
        <Button
          size='small'
          onClick={() => setMessages([])}
        >
          Clear
        </Button>
        <Button
          size='small'
          onClick={() => copyToClipboard(JSON.stringify(messages, null, 2))}
        >
          Copy
        </Button>
      </Box>
      <Card>
        <CardContent>
          {messages.length > 0 && (
            <section id='messages'>
              {messages.map((m, i) =>(
                <div key={i}>{m.msg}</div>
              ))}
            </section>
          )}
        </CardContent>
      </Card>
    </Grid>
    <Grid item xs={12} lg={4}>
      <Typography variant='h6'>Target</Typography>
      <Card>
        <CardContent>
          Mac: {target.mac}<br/>
          {target.name?.length > 0 && <>Name: {target.name}<br/></>}
          RSSI: {target.rssi}<br/>
          <FormGroup>
            <FormControlLabel
              label='Enable Hooks'
              control={
                <Switch
                  checked={hooksEnabled}
                  onChange={toggleHookState}
                />
              }
            />
          </FormGroup>
        </CardContent>
      </Card>

      {hooksEnabled && (
        <>
          <Typography variant='h6'>Read Hook</Typography>
          <HookEditor />
        
          <Typography variant='h6'>Write Hook</Typography>
          <HookEditor hook='write' />
        </>
      )}
    </Grid>
  </Grid>
));

MitmPanel.propTypes = {};

export default MitmPanel;
