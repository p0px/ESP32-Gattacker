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

import { HookEditor } from './HookEditor';

const toggleHookState = (e) => {
  window.socket.send(JSON.stringify({
    opt: 2,
    action: 'enable_hooks',
    enable: e.target.checked
  }));
};

export const MitmPanel = memo(({
  target,
  messages,
  setMessages,
  hooksEnabled = false,
}) => (
  <Grid container spacing={2}>
    <Grid size={{ xs: 12, lg: 8 }}>
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
              {messages.map((m, i) => (
                <div key={i}>{m.msg}</div>
              ))}
            </section>
          )}
        </CardContent>
      </Card>
    </Grid>
    <Grid size={{ xs: 12, lg: 4 }}>
      <Typography variant='h6'>Target</Typography>
      <Card>
        <CardContent>
          Mac: {target.mac}<br />
          {target.name?.length > 0 && <>Name: {target.name}<br /></>}
          RSSI: {target.rssi}<br />
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
