import PropTypes from 'prop-types';

import Card from '@mui/material/Card';
import Button from '@mui/material/Button';
import Typography from '@mui/material/Typography';
import CardContent from '@mui/material/CardContent';
import CardActionArea from '@mui/material/CardActionArea';

export const CancelButton = ({ onStop }) => (
  <Button
    type='submit'
    sx={{ mb: 3 }}
    variant='contained'
    color='primary'
    onClick={onStop}
  >
    Stop
  </Button>
);

CancelButton.propTypes = {};

export const Device = ({ dev, onClick }) => (
  <Card
    sx={{ maxWidth: 300 }}
    variant='outlined'
  >
    <CardActionArea onClick={onClick}>
      <CardContent>
        {dev.stations && (
          <Typography sx={{ fontSize: 14 }} color="text.secondary" gutterBottom>
            {dev.stations.length} station{dev.stations.length > 1 && 's'}
          </Typography>
        )}
        <Typography variant="h5" component="div" sx={{ overflowWrap: "break-word" }}>
          {dev.mac}
        </Typography>
        <Typography sx={{ mb: 1.5 }} color="text.secondary">
          {dev.name || dev.ssid || ""}
        </Typography>
        <hr/>
        <Typography sx={{ mb: 1.5}} variant='body2'>
          {dev.channel && (<>CH: {dev.channel}<br/></>)}
          RSSI: {dev.rssi}
        </Typography>
        {dev.stations && (
          dev.stations.map((s) => (
            <Typography key={s.mac} sx={{ overflowWrap: "break-word" }}>
              STA: {s.mac}
            </Typography>
          ))
        )}
      </CardContent>
    </CardActionArea>
  </Card>
);

Device.propTypes = {
  dev: PropTypes.object,
  onClick: PropTypes.func,
};

export const sortDevs = (a, b) => {
  // Check if 'stations' exists and get its length, otherwise default to 0
  const aSL = a.stations ? a.stations.length : 0;
  const bSL = b.stations ? b.stations.length : 0;

  // First, sort by the length of the 'stations' array if they are different
  if (bSL !== aSL) {
    return bSL - aSL;
  }

  // If the lengths are the same, sort by 'rssi'
  return b.rssi - a.rssi;
};
