// React
import React, { useState, useEffect } from 'react'
import {
  Outlet,
  createBrowserRouter,
  RouterProvider,
  useNavigate,
} from "react-router-dom";

// MUI Components
import { styled } from '@mui/material/styles';
import Box from '@mui/material/Box';
import MuiDrawer from '@mui/material/Drawer';
import MuiAppBar from '@mui/material/AppBar';
import Toolbar from '@mui/material/Toolbar';
import List from '@mui/material/List';
import Typography from '@mui/material/Typography';
import Divider from '@mui/material/Divider';
import IconButton from '@mui/material/IconButton';
import MenuIcon from '@mui/icons-material/Menu';
import SettingsIcon from '@mui/icons-material/Settings';
import ChevronLeftIcon from '@mui/icons-material/ChevronLeft';
import ListItem from '@mui/material/ListItem';
import ListItemButton from '@mui/material/ListItemButton';
import ListItemIcon from '@mui/material/ListItemIcon';
import ListItemText from '@mui/material/ListItemText';
import HomeIcon from '@mui/icons-material/Home';
import InfoIcon from '@mui/icons-material/Info';
import Container from '@mui/material/Container';
import Grid from '@mui/material/Grid';
import Alert from '@mui/material/Alert';

// Custom Components
import Home from './components/Home';
import Settings from './components/Settings';
import About from './components/About';

const drawerWidth = 200;

const openedMixin = (theme) => ({
  width: drawerWidth,
  transition: theme.transitions.create('width', {
    easing: theme.transitions.easing.sharp,
    duration: theme.transitions.duration.enteringScreen,
  }),
  overflowX: 'hidden',
});

const closedMixin = (theme) => ({
  transition: theme.transitions.create('width', {
    easing: theme.transitions.easing.sharp,
    duration: theme.transitions.duration.leavingScreen,
  }),
  overflowX: 'hidden',
  width: `calc(${theme.spacing(7)} + 1px)`,
  [theme.breakpoints.up('sm')]: {
    width: `calc(${theme.spacing(8)} + 1px)`,
  },
});

const DrawerHeader = styled('div')(({ theme }) => ({
  display: 'flex',
  alignItems: 'center',
  justifyContent: 'flex-end',
  padding: theme.spacing(0, 1),
  // necessary for content to be below app bar
  ...theme.mixins.toolbar,
}));

const AppBar = styled(MuiAppBar, {
  shouldForwardProp: (prop) => prop !== 'open',
})(({ theme, open }) => ({
  zIndex: theme.zIndex.drawer + 1,
  transition: theme.transitions.create(['width', 'margin'], {
    easing: theme.transitions.easing.sharp,
    duration: theme.transitions.duration.leavingScreen,
  }),
  ...(open && {
    marginLeft: drawerWidth,
    width: `calc(100% - ${drawerWidth}px)`,
    transition: theme.transitions.create(['width', 'margin'], {
      easing: theme.transitions.easing.sharp,
      duration: theme.transitions.duration.enteringScreen,
    }),
  }),
}));

const Drawer = styled(MuiDrawer, { shouldForwardProp: (prop) => prop !== 'open' })(
  ({ theme, open }) => ({
    width: drawerWidth,
    flexShrink: 0,
    whiteSpace: 'nowrap',
    boxSizing: 'border-box',
    ...(open && {
      ...openedMixin(theme),
      '& .MuiDrawer-paper': openedMixin(theme),
    }),
    ...(!open && {
      ...closedMixin(theme),
      '& .MuiDrawer-paper': closedMixin(theme),
    }),
  }),
);

export const Layout = () => {
  const navigate = useNavigate();
  const [open, setOpen] = useState(false);
  const [msg, setMsg] = useState('');
  const [alert, setAlert] = useState('success');
  const [showAlert, setShowAlert] = useState(false)

  const closeAlert = () => {
    setShowAlert(false);
    setMsg('');
  };

  const setupSocket = () => {
    if (!window.socket || window.socket.readyState === 3) {
      const socket = new WebSocket(`ws://1.3.3.7/ws`);

      socket.onopen = () => {
        console.log('WebSocket connection established');
        setShowAlert(false);
        setMsg('');

        if (window.socketCallback) {
          window.socketCallback({ connected: true });
        }
      };

      socket.onmessage = (message) => { 
        console.log('Message from server ', JSON.parse(message.data));
        const res = JSON.parse(message.data);

        if (res.hasOwnProperty('success')) {
          if (res.success == false) {
            setMsg(res.error);
            setAlert('error');
            setShowAlert(true);
          } else {
            setMsg('Success');
            setAlert('success');
            setShowAlert(true);

            if (window.onOptSuccess) {
              window.onOptSuccess();
            }
          }
        }

        if (window.socketCallback) {
          window.socketCallback(res);
        }
      };

      socket.onerror = (error) => {
        console.error('WebSocket error: ', error);
      };

      socket.onclose = () => {
        console.log('WebSocket connection closed');
        setMsg('No Connection');
        setAlert('error');
        setShowAlert(true);

        setTimeout(() => {
          setupSocket();
        }, 500);
      };

      window.socket = socket;  
    }
  };

  useEffect(() => {
    setupSocket();

    return () => {
      window.socket.close();
    };
  }, []);

  return (
    <Box sx={{ display: 'flex' }}>
      <AppBar position='fixed' open={open}>
        <Toolbar>
          <IconButton
            color='inherit'
            aria-label='open drawer'
            onClick={() => setOpen(true)}
            edge='start'
            sx={{
              marginRight: 5,
              ...(open && { display: 'none' }),
            }}
          >
            <MenuIcon />
          </IconButton>
          <Typography variant='h6' noWrap component='div'>
            ESP32-Gattacker
          </Typography>
        </Toolbar>
      </AppBar>
      <Drawer variant='permanent' open={open}>
        <DrawerHeader>
          <IconButton onClick={() => setOpen(false)}>
            <ChevronLeftIcon />
          </IconButton>
        </DrawerHeader>
        <Divider />
        <List>
          {['Home', 'Settings', 'About'].map((text, i) => (
            <React.Fragment key={text}>
              { i === 1 && <Divider />}
              <ListItem key={text} disablePadding sx={{ display: 'block' }}>
                <ListItemButton
                  selected={window.location.pathname.split('/')[1] === `${text.toLowerCase()}`}
                  onClick={() => navigate(`/${text.toLowerCase()}`)}
                  sx={{
                    minHeight: 48,
                    justifyContent: open ? 'initial' : 'center',
                    px: 2.5,
                  }}
                >
                  <ListItemIcon
                    sx={{
                      minWidth: 0,
                      mr: open ? 3 : 'auto',
                      justifyContent: 'center',
                    }}
                  >
                    {i === 0 && <HomeIcon />}
                    {i === 1 && <SettingsIcon />}
                    {i === 2 && <InfoIcon />}
                  </ListItemIcon>
                  <ListItemText primary={text} sx={{ opacity: open ? 1 : 0 }} />
                </ListItemButton>
              </ListItem>
            </React.Fragment>
          ))}
        </List>
      </Drawer>
      <Box component='main' sx={{ flexGrow: 1, p: 3 }}>
        <DrawerHeader />
        <Container disableGutters maxWidth={false}>
          <Grid container>
            <Grid item xs={12}>
              {showAlert && (
                <Alert
                  severity={alert}
                  onClose={closeAlert}
                >
                  {msg}
                </Alert>
              )}

              <Outlet />
            </Grid>
          </Grid>
        </Container>
      </Box>
    </Box>
  );
};

const router = createBrowserRouter([
  {
    path: "/",
    element: <Layout />,
    children: [
      {
        index: true,
        element: <Home />,
      },
      {
        path: "home",
        element: <Home />
      },
      {
        path: "settings",
        element: <Settings />
      },
      {
        path: "about",
        element: <About />
      },
    ],
  },
]);

export function App() {
  return <RouterProvider router={router} fallbackElement={<p>Loading...</p>} />;
}

export default App;
