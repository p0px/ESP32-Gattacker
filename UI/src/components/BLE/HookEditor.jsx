import { useState } from 'react';
import Card from '@mui/material/Card';
import CardContent from '@mui/material/CardContent';
import Typography from '@mui/material/Typography';

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

if (!window.hook_read) window.hook_read = eval(defaultRead);
if (!window.hook_write) window.hook_write = eval(defaultWrite);

export const HookEditor = ({
  hook = 'read'
}) => {
  const [error, setError] = useState(false);
  const [code, setCode] = useState(window[`hook_${hook}`]?.toString() || (hook === 'read' ? defaultRead : defaultWrite));

  const handleChange = (e) => {
    setCode(e.target.value);
    try {
      const test = eval(`(${e.target.value})`)({ uuid: 'test', 'value': '01020304' });
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

export default HookEditor;
