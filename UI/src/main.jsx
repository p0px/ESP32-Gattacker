import React from 'react'
import ReactDOM from 'react-dom/client'

import {
  createTheme,
  ThemeProvider,
  CssBaseline,
} from '@mui/material';

import App from './App.jsx'

import './index.css'

const theme = createTheme({
  palette: {
    mode: 'dark',
    primary: {
      main: '#0f0',
    },
    background: {
     default: '#312f2f',
     paper: '#333',    
    },    
  },
  typography: {
    fontFamily: 'monospace', 
  },
});

ReactDOM.createRoot(document.getElementById('root')).render(
  <React.StrictMode>
    <ThemeProvider theme={theme}>
      <CssBaseline />
      <App />
    </ThemeProvider>
  </React.StrictMode>
);
